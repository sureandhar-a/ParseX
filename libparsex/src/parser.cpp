#include <parsex/parser/parser.hpp>

#include <parsex/parser/loader.hpp>
#include <parsex/parser/model_builder.hpp>
#include <parsex/parser/project_error.hpp>
#include <parsex/parser/release_detector.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Dispatches one raw node into its domain vector (recursed over the whole
// tree below). Tags outside the six families are skipped — other AUTOSAR
// content (companies, mappings, COMPU-METHODs, ...) is not ParseX domain data.
void buildSubtree(const RawNode& node, ParsedFile& file) {
    const std::string& tag = node.tagName;
    if (tag == "CLUSTER" || tag == "CAN-CLUSTER") {
        file.clusters.push_back(buildCluster(node, file.warnings));
    } else if (tag == "ECU-INSTANCE") {
        file.ecuInstances.push_back(buildEcuInstance(node, file.warnings));
    } else if (tag == "FRAME" || tag == "CAN-FRAME") {
        file.frames.push_back(buildFrame(node, file.warnings));
    } else if (tag == "PDU" || tag == "I-SIGNAL-I-PDU") {
        file.pdus.push_back(buildPdu(node, file.warnings));
    } else if (tag == "SYSTEM-SIGNAL" || tag == "I-SIGNAL") {
        file.signals.push_back(buildSignal(node, file.warnings));
    } else if (tag == "SIGNAL-GROUP" || tag == "I-SIGNAL-GROUP") {
        file.signalGroups.push_back(buildSignalGroup(node, file.warnings));
    }
    for (const auto& child : node.children) {
        buildSubtree(*child, file);
    }
}

// Case-insensitive: some tools emit .ARXML / .ArXml.
bool isArxmlFile(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& letter : extension) {
        letter =
            static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    }
    return extension == ".arxml";
}

// ---- LazyOnReference support ----

// Hard backstop on the expansion loop: every productive iteration parses at
// least one new file out of a finite candidate set, and a fruitless iteration
// stops the loop outright — so termination never actually depends on this.
// It exists so a pathological setup fails loudly instead of hanging the CLI.
constexpr int kMaxLazyIterations = 50;

// Short names still missing from the project, by the domain type that must
// define them. Only reference targets that ARE domain objects are chased:
// CHANNEL-REFs and controller names resolve to nested (non-domain) elements
// that no file "defines" at project level, so chasing them could never
// terminate successfully.
struct WantedRefs {
    std::set<std::string> pdus;
    std::set<std::string> signals;
    std::set<std::string> ecus;
    [[nodiscard]] bool empty() const {
        return pdus.empty() && signals.empty() && ecus.empty();
    }
};

void wantName(std::set<std::string>& wanted, const std::set<std::string>& known,
              const std::string& name) {
    if (!name.empty() && !known.contains(name)) {
        wanted.insert(name);
    }
}

struct KnownRefs {
    std::set<std::string> pdus;
    std::set<std::string> signals;
    std::set<std::string> ecus;
};

KnownRefs collectKnownRefs(const ParsedProject& project) {
    KnownRefs known;
    for (const auto& file : project.files) {
        for (const auto& pdu : file.pdus) {
            known.pdus.insert(pdu.common.shortName);
        }
        for (const auto& signal : file.signals) {
            known.signals.insert(signal.common.shortName);
        }
        for (const auto& ecu : file.ecuInstances) {
            known.ecus.insert(ecu.common.shortName);
        }
    }
    return known;
}

void wantFrameRefs(const Frame& frame, const KnownRefs& known, WantedRefs& wanted) {
    for (const auto& transmitter : frame.transmitters) {
        wantName(wanted.ecus, known.ecus, transmitter);
    }
    for (const auto& mapping : frame.pdus) {
        wantName(wanted.pdus, known.pdus, mapping.pduShortNameRef);
    }
}

void wantFileRefs(const ParsedFile& file, const KnownRefs& known, WantedRefs& wanted) {
    for (const auto& frame : file.frames) {
        wantFrameRefs(frame, known, wanted);
    }
    for (const auto& pdu : file.pdus) {
        for (const auto& mapping : pdu.signalMappings) {
            wantName(wanted.signals, known.signals, mapping.signalShortNameRef);
        }
    }
    for (const auto& signal : file.signals) {
        for (const auto& receiver : signal.receivers) {
            wantName(wanted.ecus, known.ecus, receiver);
        }
    }
    for (const auto& group : file.signalGroups) {
        for (const auto& member : group.members) {
            wantName(wanted.signals, known.signals, member);
        }
    }
}

WantedRefs collectWantedRefs(const ParsedProject& project) {
    const KnownRefs known = collectKnownRefs(project);
    WantedRefs wanted;
    for (const auto& file : project.files) {
        wantFileRefs(file, known, wanted);
    }
    return wanted;
}

bool isSpaceByte(unsigned char unit) {
    return unit == ' ' || unit == '\t' || unit == '\r' || unit == '\n';
}

// Cheap plausibility pre-filter: does the raw file contain an exact
// `<SHORT-NAME>name</...` (or prefixed `<p:SHORT-NAME>name</...`) element?
// Single-byte encodings only — UTF-16 files (BOM) always parse to decide
// exactly. False positives (comments mentioning the name) are harmless: the
// parse below decides exactly. False negatives would be DanglingFileReference
// lies, hence the whitespace tolerance and the UTF-16 fallback.
bool rawDefinesShortName(const std::string& bytes, const std::string& shortName) {
    if (bytes.size() >= 2) {
        const auto first = static_cast<unsigned char>(bytes.at(0));
        const auto second = static_cast<unsigned char>(bytes.at(1));
        if ((first == 0xFF && second == 0xFE) || (first == 0xFE && second == 0xFF)) {
            return true;
        }
    }
    const std::string opener = "SHORT-NAME>";
    std::size_t pos = 0;
    while ((pos = bytes.find(opener, pos)) != std::string::npos) {
        if (pos == 0 || (bytes.at(pos - 1) != '<' && bytes.at(pos - 1) != ':')) {
            ++pos;
            continue;
        }
        std::size_t namePos = pos + opener.size();
        while (namePos < bytes.size() && isSpaceByte(static_cast<unsigned char>(bytes.at(namePos)))) {
            ++namePos;
        }
        if (bytes.compare(namePos, shortName.size(), shortName) != 0) {
            pos = namePos;
            continue;
        }
        std::size_t after = namePos + shortName.size();
        while (after < bytes.size() && isSpaceByte(static_cast<unsigned char>(bytes.at(after)))) {
            ++after;
        }
        if (after < bytes.size() && bytes.at(after) == '<') {
            return true;
        }
        pos = after;
    }
    return false;
}

std::string readFileBytesOrEmpty(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return "";
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<std::string> flattenWanted(const WantedRefs& wanted) {
    std::vector<std::string> names;
    for (const auto& nameSet : {wanted.pdus, wanted.signals, wanted.ecus}) {
        names.insert(names.end(), nameSet.begin(), nameSet.end());
    }
    return names;
}

bool candidateDefinesAny(const std::filesystem::path& path, const WantedRefs& wanted) {
    const std::string bytes = readFileBytesOrEmpty(path);
    if (bytes.empty()) {
        return true;  // Unreadable here; let the parse attempt decide (and skip).
    }
    const std::vector<std::string> names = flattenWanted(wanted);
    return std::ranges::any_of(names, [&](const std::string& name) {
        return rawDefinesShortName(bytes, name);
    });
}

bool definesWantedName(const ParsedFile& file, const WantedRefs& wanted) {
    const auto definesPdu = [&](const Pdu& pdu) {
        return wanted.pdus.contains(pdu.common.shortName);
    };
    const auto definesSignal = [&](const Signal& signal) {
        return wanted.signals.contains(signal.common.shortName);
    };
    const auto definesEcu = [&](const EcuInstance& ecu) {
        return wanted.ecus.contains(ecu.common.shortName);
    };
    return std::ranges::any_of(file.pdus, definesPdu) ||
           std::ranges::any_of(file.signals, definesSignal) ||
           std::ranges::any_of(file.ecuInstances, definesEcu);
}

std::filesystem::path normalizePath(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal();
}

// ---- Final cross-file resolution ----
//
// Runs identically at the end of every discovery mode. Builds a per-type
// index over all domain objects (short name -> defined; first file order
// wins on same-name collisions — the Validator's future duplicate rule owns
// those), then records one ResolvedReference per reference site.
//
// Site keys read "<OwnerShortName>.<field>[<index>]" (e.g.
// "Frame_1.pdus[0]"): unique per site, deterministic, human-greppable. Values
// are the referenced short names verbatim — full AUTOSAR-path tracking is
// future work (the Builder reduces refs to short names, so paths are not
// recoverable here); the map's presence per site already proves resolvability.
//
// Only the five domain-object-targeting field families participate — the same
// set LazyOnReference chases, so a lazily-completed project always passes
// this final check. In particular, refs to existing-but-unbuilt types
// (MULTIPLEXED-I-PDU, NM-PDU, ...) dangle here by design: only the six built
// families form the index.

struct UnresolvedSite {
    std::string key;
    std::string wanted;
};

void resolveFileRefs(const ParsedFile& file, const KnownRefs& known,
                     std::map<std::string, ResolvedReference>& resolved,
                     std::vector<UnresolvedSite>& unresolved) {
    const auto resolve = [&](const std::string& key, const std::string& wanted,
                             const std::set<std::string>& knownNames) {
        if (wanted.empty()) {
            return;  // Malformed ref with no target: not file-resolvable.
        }
        if (knownNames.contains(wanted)) {
            resolved.try_emplace(key, ResolvedReference{wanted});
        } else {
            unresolved.push_back({.key = key, .wanted = wanted});
        }
    };
    for (const auto& frame : file.frames) {
        for (std::size_t idx = 0; idx < frame.transmitters.size(); ++idx) {
            resolve(frame.common.shortName + ".transmitters[" + std::to_string(idx) + "]",
                    frame.transmitters.at(idx), known.ecus);
        }
        for (std::size_t idx = 0; idx < frame.pdus.size(); ++idx) {
            resolve(frame.common.shortName + ".pdus[" + std::to_string(idx) + "]",
                    frame.pdus.at(idx).pduShortNameRef, known.pdus);
        }
    }
    for (const auto& pdu : file.pdus) {
        for (std::size_t idx = 0; idx < pdu.signalMappings.size(); ++idx) {
            resolve(pdu.common.shortName + ".signalMappings[" + std::to_string(idx) + "]",
                    pdu.signalMappings.at(idx).signalShortNameRef, known.signals);
        }
    }
    for (const auto& signal : file.signals) {
        for (std::size_t idx = 0; idx < signal.receivers.size(); ++idx) {
            resolve(signal.common.shortName + ".receivers[" + std::to_string(idx) + "]",
                    signal.receivers.at(idx), known.ecus);
        }
    }
    for (const auto& group : file.signalGroups) {
        for (std::size_t idx = 0; idx < group.members.size(); ++idx) {
            resolve(group.common.shortName + ".members[" + std::to_string(idx) + "]",
                    group.members.at(idx), known.signals);
        }
    }
}

void resolveCrossFileReferences(ParsedProject& project) {
    const KnownRefs known = collectKnownRefs(project);
    std::map<std::string, ResolvedReference> resolved;
    std::vector<UnresolvedSite> unresolved;
    for (const auto& file : project.files) {
        resolveFileRefs(file, known, resolved, unresolved);
    }
    if (!unresolved.empty()) {
        std::set<std::string> missing;
        std::string sites;
        for (const auto& site : unresolved) {
            missing.insert(site.wanted);
            if (!sites.empty()) {
                sites += "; ";
            }
            sites += site.key + " -> " + site.wanted;
        }
        std::vector<std::filesystem::path> dirs;
        for (const auto& file : project.files) {
            std::filesystem::path directory = file.sourcePath.parent_path();
            if (directory.empty()) {
                directory = ".";
            }
            if (std::ranges::find(dirs, directory) == dirs.end()) {
                dirs.push_back(directory);
            }
        }
        throw DanglingFileReferenceError(
            std::vector<std::string>(missing.begin(), missing.end()), std::move(dirs),
            sites);
    }
    project.resolvedRefs = std::move(resolved);
}

}  // namespace

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): stateless-by-design instance API — callers write Parser{}.parseFile(...).
ParsedFile Parser::parseFile(const std::filesystem::path& path) const {
    RawDocument document = loadRawDocument(path);

    const std::optional<std::string> schemaFilename = detectSchemaFilename(document);
    if (!schemaFilename.has_value()) {
        throw std::runtime_error("parsex: cannot determine AUTOSAR release for '" +
                                 path.string() +
                                 "': root element has no xsi:schemaLocation");
    }

    ParsedFile file;
    file.autosarRelease = resolveRelease(schemaFilename.value());
    file.sourcePath = path;
    buildSubtree(document.root, file);
    // Document move re-points parent links at the shared copy (see
    // RawDocument's move operations) — the Write Engine's spans stay valid.
    file.rawDocument = std::make_shared<RawDocument>(std::move(document));
    return file;
}

ParsedProject Parser::parseProject(const std::vector<std::filesystem::path>& entryPoints,
                                   FileDiscoveryMode mode) const {
    if (mode == FileDiscoveryMode::LazyOnReference) {
        return parseProjectLazy(entryPoints);
    }
    if (mode == FileDiscoveryMode::ExplicitList) {
        ParsedProject project;
        for (const auto& entry : entryPoints) {
            // Deliberately no try/catch: one failing file fails the whole call
            // (see header). Partial results are discarded with the project.
            project.files.push_back(parseFile(entry));
        }
        resolveCrossFileReferences(project);
        return project;
    }
    // DirectoryScan: every .arxml sibling of every entry point (single level,
    // non-recursive by design — see FileDiscoveryMode), plus the entry points
    // themselves. Absolute normalized paths dedupe shared directories and keep
    // ParsedProject.files in deterministic (sorted) order.
    std::set<std::filesystem::path> discovered;
    for (const auto& entry : entryPoints) {
        discovered.insert(std::filesystem::absolute(entry).lexically_normal());
        std::filesystem::path directory = entry.parent_path();
        if (directory.empty()) {
            directory = std::filesystem::current_path();
        }
        for (const auto& sibling : std::filesystem::directory_iterator(directory)) {
            if (sibling.is_regular_file() && isArxmlFile(sibling.path())) {
                discovered.insert(
                    std::filesystem::absolute(sibling.path()).lexically_normal());
            }
        }
    }
    ParsedProject project;
    for (const auto& path : discovered) {
        project.files.push_back(parseFile(path));
    }
    resolveCrossFileReferences(project);
    return project;
}

// Candidate pool, enumerated once up front: recursive this time (unlike
// DirectoryScan), because references routinely point deeper. Sorted for
// deterministic discovery order; already-parsed files excluded.
std::vector<std::filesystem::path> enumerateCandidates(
    const std::vector<std::filesystem::path>& entryPoints,
    const std::set<std::filesystem::path>& parsed) {
    std::set<std::filesystem::path> searchDirs;
    for (const auto& entry : entryPoints) {
        const std::filesystem::path directory = entry.parent_path();
        searchDirs.insert(directory.empty() ? std::filesystem::current_path()
                                            : normalizePath(directory));
    }
    std::vector<std::filesystem::path> candidates;
    for (const auto& directory : searchDirs) {
        for (const auto& candidate :
             std::filesystem::recursive_directory_iterator(directory)) {
            if (candidate.is_regular_file() && isArxmlFile(candidate.path())) {
                const std::filesystem::path normalized = normalizePath(candidate.path());
                if (!parsed.contains(normalized)) {
                    candidates.push_back(normalized);
                }
            }
        }
    }
    std::ranges::sort(candidates);
    return candidates;
}

enum class ExpansionStep : std::uint8_t { Complete, Advanced, Stuck };

// One fixpoint round: parse every candidate that plausibly defines a still-
// missing name. Complete = nothing missing; Advanced = project grew (scan
// again — the new files may want more); Stuck = nothing left to find.
template <typename ParseFileFn>
ExpansionStep expandProjectOnce(ParsedProject& project,
                                const std::vector<std::filesystem::path>& candidates,
                                std::set<std::filesystem::path>& parsed,
                                const ParseFileFn& parse) {
    const WantedRefs wanted = collectWantedRefs(project);
    if (wanted.empty()) {
        return ExpansionStep::Complete;
    }
    bool added = false;
    for (const auto& candidate : candidates) {
        if (parsed.contains(candidate)) {
            continue;
        }
        if (!candidateDefinesAny(candidate, wanted)) {
            continue;
        }
        try {
            ParsedFile file = parse(candidate);
            if (!definesWantedName(file, wanted)) {
                continue;  // Pre-filter false positive (e.g. name in a comment).
            }
            parsed.insert(candidate);
            project.files.push_back(std::move(file));
            added = true;
        } catch (const std::exception&) {
            // Discovery probes opportunistically: an unparseable candidate is
            // skipped, not fatal. If it was actually required, its ref stays
            // unresolved and surfaces below as DanglingFileReference.
            continue;
        }
    }
    return added ? ExpansionStep::Advanced : ExpansionStep::Stuck;
}

ParsedProject Parser::parseProjectLazy(const std::vector<std::filesystem::path>& entryPoints) const {
    ParsedProject project;
    std::set<std::filesystem::path> parsed;
    for (const auto& entry : entryPoints) {
        project.files.push_back(parseFile(entry));
        parsed.insert(normalizePath(entry));
    }
    const std::vector<std::filesystem::path> candidates =
        enumerateCandidates(entryPoints, parsed);

    for (int iteration = 0; iteration < kMaxLazyIterations; ++iteration) {
        const ExpansionStep step = expandProjectOnce(
            project, candidates, parsed,
            [this](const std::filesystem::path& path) { return parseFile(path); });
        if (step != ExpansionStep::Advanced) {
            break;
        }
    }
    // Same final check as every other mode (with per-site details): the
    // expansion above already threw DanglingFileReferenceError for anything
    // it could not pull in, so this only ever fires past the iteration cap.
    resolveCrossFileReferences(project);
    return project;
}

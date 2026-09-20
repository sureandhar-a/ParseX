#include <parsex/parser/loader.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <libxml/parser.h>

namespace {

// Raw-byte width of one markup character: single-byte encodings (UTF-8,
// Latin-1, ...) use one byte per unit, UTF-16 two. The boundary scanner below
// works in whole units; only it needs the width — element/attribute *values*
// come from libxml2 already transcoded.
enum class RawEncodingClass : std::uint8_t { SingleByte, Utf16Le, Utf16Be };

unsigned char byteAt(const std::string& bytes, std::size_t idx) {
    return static_cast<unsigned char>(bytes.at(idx));
}

std::size_t unitWidth(RawEncodingClass encoding) {
    return encoding == RawEncodingClass::SingleByte ? 1 : 2;
}

std::string readFileBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("parsex: cannot open file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string asciiUpper(std::string text) {
    for (char& letter : text) {
        if (letter >= 'a' && letter <= 'z') {
            letter = static_cast<char>(letter - ('a' - 'A'));
        }
    }
    return text;
}

bool startsWith(const std::string& bytes, std::initializer_list<unsigned char> prefix) {
    if (bytes.size() < prefix.size()) {
        return false;
    }
    std::size_t idx = 0;
    for (unsigned char expected : prefix) {
        if (byteAt(bytes, idx) != expected) {
            return false;
        }
        ++idx;
    }
    return true;
}

// BOM sniff. UTF-32 BOMs are rejected outright: ARXML never uses UTF-32, and
// silently mis-scanning it would corrupt every span.
std::optional<RawEncodingClass> bomEncodingClass(const std::string& bytes) {
    if (startsWith(bytes, {0xFF, 0xFE, 0x00, 0x00}) ||
        startsWith(bytes, {0x00, 0x00, 0xFE, 0xFF})) {
        throw std::runtime_error("parsex: UTF-32 input is not supported");
    }
    if (startsWith(bytes, {0xFF, 0xFE})) {
        return RawEncodingClass::Utf16Le;
    }
    if (startsWith(bytes, {0xFE, 0xFF})) {
        return RawEncodingClass::Utf16Be;
    }
    return std::nullopt;
}

// First-non-whitespace '<' sniff for BOM-less UTF-16: the XML declaration
// must start the file, so its first '<' is either 3C 00 (LE) or 00 3C (BE).
RawEncodingClass sniffLessThanEndianness(const std::string& bytes) {
    std::size_t idx = 0;
    while (idx < bytes.size() && (byteAt(bytes, idx) == 0x20 || byteAt(bytes, idx) == 0x09 ||
                                  byteAt(bytes, idx) == 0x0D || byteAt(bytes, idx) == 0x0A)) {
        ++idx;
    }
    if (idx + 1 < bytes.size()) {
        if (byteAt(bytes, idx) == 0x3C && byteAt(bytes, idx + 1) == 0x00) {
            return RawEncodingClass::Utf16Le;
        }
        if (byteAt(bytes, idx) == 0x00 && byteAt(bytes, idx + 1) == 0x3C) {
            return RawEncodingClass::Utf16Be;
        }
    }
    return RawEncodingClass::SingleByte;
}

// ASCII sniff of the XML declaration's encoding="..." (the declaration is
// pure ASCII in every single-byte encoding; BOM'd UTF-16 never reaches here).
RawEncodingClass declarationEncodingClass(const std::string& bytes) {
    const std::string head = bytes.substr(0, std::min(bytes.size(), std::size_t{300}));
    const std::size_t keyPos = head.find("encoding");
    if (keyPos == std::string::npos) {
        return RawEncodingClass::SingleByte;
    }
    const std::size_t quotePos = head.find_first_of("\"'", keyPos);
    if (quotePos == std::string::npos || quotePos + 1 >= head.size()) {
        return RawEncodingClass::SingleByte;
    }
    const std::size_t endQuote = head.find(head.at(quotePos), quotePos + 1);
    const std::string declared =
        asciiUpper(head.substr(quotePos + 1, endQuote - quotePos - 1));
    if (declared.find("UTF-16") == std::string::npos) {
        return RawEncodingClass::SingleByte;
    }
    if (declared.find("LE") != std::string::npos) {
        return RawEncodingClass::Utf16Le;
    }
    if (declared.find("BE") != std::string::npos) {
        return RawEncodingClass::Utf16Be;
    }
    return sniffLessThanEndianness(bytes);
}

// Encoding-class detection from raw bytes, cheapest reliable signal first.
RawEncodingClass detectEncodingClass(const std::string& bytes) {
    const std::optional<RawEncodingClass> bomResult = bomEncodingClass(bytes);
    if (bomResult.has_value()) {
        return bomResult.value();
    }
    return declarationEncodingClass(bytes);
}

// ASCII code of the unit at scanPos, or -1 when out of range or non-ASCII.
// Non-ASCII markup units only occur inside attribute values and text, never
// where the scanner makes decisions, so -1 safely means "not markup".
int unitAscii(const std::string& bytes, RawEncodingClass encoding, std::size_t scanPos) {
    if (encoding == RawEncodingClass::SingleByte) {
        if (scanPos >= bytes.size()) {
            return -1;
        }
        const unsigned char unit = byteAt(bytes, scanPos);
        return unit < 0x80 ? static_cast<int>(unit) : -1;
    }
    if (scanPos + 1 >= bytes.size()) {
        return -1;
    }
    if (encoding == RawEncodingClass::Utf16Le) {
        return byteAt(bytes, scanPos + 1) == 0
                   ? static_cast<int>(byteAt(bytes, scanPos))
                   : -1;
    }
    return byteAt(bytes, scanPos) == 0 ? static_cast<int>(byteAt(bytes, scanPos + 1)) : -1;
}

bool unitIs(const std::string& bytes, RawEncodingClass encoding, std::size_t scanPos,
            char expected) {
    return unitAscii(bytes, encoding, scanPos) == static_cast<int>(expected);
}

bool unitIsSpace(const std::string& bytes, RawEncodingClass encoding, std::size_t scanPos) {
    const int code = unitAscii(bytes, encoding, scanPos);
    return code == ' ' || code == '\t' || code == '\r' || code == '\n';
}

bool matchLiteral(const std::string& bytes, RawEncodingClass encoding, std::size_t scanPos,
                  std::string_view literal) {
    const std::size_t width = unitWidth(encoding);
    for (std::size_t idx = 0; idx < literal.size(); ++idx) {
        if (!unitIs(bytes, encoding, scanPos + (idx * width), literal.at(idx))) {
            return false;
        }
    }
    return true;
}

void requireAsciiName(const std::string& tagName) {
    // The scanner matches raw units against ASCII. AUTOSAR element names are
    // ASCII by construction; anything else fails loud rather than mis-scanning.
    for (char letter : tagName) {
        if (static_cast<unsigned char>(letter) >= 0x80) {
            throw std::runtime_error("parsex: non-ASCII tag name is not supported: " +
                                     tagName);
        }
    }
}

// Past the closing delimiter of a comment / PI / CDATA section opened at
// openPos ('<'). Each construct ends at its first terminator, matching
// libxml2's leniency closely enough that any divergence trips the
// name assertions in the tag scanners below (loud, never silently wrong).
std::size_t skipDelimited(const std::string& bytes, RawEncodingClass encoding,
                          std::size_t openPos, std::string_view terminator) {
    const std::size_t width = unitWidth(encoding);
    std::size_t scanPos = openPos;
    while (scanPos < bytes.size()) {
        if (matchLiteral(bytes, encoding, scanPos, terminator)) {
            return scanPos + (terminator.size() * width);
        }
        scanPos += width;
    }
    throw std::runtime_error("parsex: unterminated markup while scanning raw bytes");
}

// Past the quoted value opened at quotePos (the opening quote unit).
std::size_t skipQuoted(const std::string& bytes, RawEncodingClass encoding,
                       std::size_t quotePos) {
    const int quote = unitAscii(bytes, encoding, quotePos);
    const std::size_t width = unitWidth(encoding);
    std::size_t scanPos = quotePos + width;
    while (scanPos < bytes.size()) {
        if (unitAscii(bytes, encoding, scanPos) == quote) {
            return scanPos + width;
        }
        scanPos += width;
    }
    throw std::runtime_error("parsex: unterminated quoted value while scanning raw bytes");
}

// Past a <!DOCTYPE / <!ENTITY / ... declaration opened at openPos ('<'):
// '>' ends it at bracket depth zero, quotes and [...] nesting respected.
std::size_t skipDeclaration(const std::string& bytes, RawEncodingClass encoding,
                            std::size_t openPos) {
    const std::size_t width = unitWidth(encoding);
    int depth = 0;
    std::size_t scanPos = openPos + width;
    while (scanPos < bytes.size()) {
        const int code = unitAscii(bytes, encoding, scanPos);
        if (code == '"' || code == '\'') {
            scanPos = skipQuoted(bytes, encoding, scanPos);
            continue;
        }
        if (code == '[') {
            ++depth;
        } else if (code == ']') {
            --depth;
        } else if (code == '>' && depth == 0) {
            return scanPos + width;
        }
        scanPos += width;
    }
    throw std::runtime_error("parsex: unterminated declaration while scanning raw bytes");
}

// Skips one non-element unit opened at openPos ('<'): comment, PI, CDATA
// section, or other <! declaration. Returns false when openPos is not such a
// unit (plain element markup or end tag); otherwise returns true and the
// position past the unit in skippedPos.
bool skipSpecialUnit(const std::string& bytes, RawEncodingClass encoding, std::size_t openPos,
                     std::size_t& skippedPos) {
    const std::size_t width = unitWidth(encoding);
    if (unitIs(bytes, encoding, openPos + width, '?')) {
        skippedPos = skipDelimited(bytes, encoding, openPos, "?>");
        return true;
    }
    if (!unitIs(bytes, encoding, openPos + width, '!')) {
        return false;
    }
    if (matchLiteral(bytes, encoding, openPos, "<!--")) {
        skippedPos = skipDelimited(bytes, encoding, openPos, "-->");
    } else if (matchLiteral(bytes, encoding, openPos, "<![CDATA[")) {
        skippedPos = skipDelimited(bytes, encoding, openPos, "]]>");
    } else {
        skippedPos = skipDeclaration(bytes, encoding, openPos);
    }
    return true;
}

struct StartTagBounds {
    std::size_t openPos;    // raw offset of '<'
    std::size_t tagEndPos;  // raw offset just past '>' (or '/>')
    bool selfClosed;
};

// Scans forward from cursor for `<tagName ...>` (or `<tagName .../>`),
// skipping text (raw '<' cannot appear in text), comments, PIs, CDATA
// sections, and declarations. Every other '<' shape throws: SAX events arrive
// in document order, so the next markup MUST be this tag or a skippable unit
// — a mismatch means scanner/parser divergence, which must fail loud.
StartTagBounds scanStartTag(const std::string& bytes, RawEncodingClass encoding,
                            std::size_t cursor, const std::string& tagName) {
    requireAsciiName(tagName);
    const std::size_t width = unitWidth(encoding);
    std::size_t scanPos = cursor;
    while (scanPos < bytes.size()) {
        if (!unitIs(bytes, encoding, scanPos, '<')) {
            scanPos += width;
            continue;
        }
        std::size_t skippedPos = 0;
        if (skipSpecialUnit(bytes, encoding, scanPos, skippedPos)) {
            scanPos = skippedPos;
            continue;
        }
        if (unitIs(bytes, encoding, scanPos + width, '/')) {
            throw std::runtime_error("parsex: unexpected end tag while seeking <" + tagName +
                                     ">");
        }
        const std::size_t namePos = scanPos + width;
        if (!matchLiteral(bytes, encoding, namePos, tagName)) {
            throw std::runtime_error("parsex: unexpected markup while seeking <" + tagName +
                                     ">");
        }
        const std::size_t afterName = namePos + (tagName.size() * width);
        // A shared prefix must not match: AR-PACKAGE is not AR-PACKAGES.
        if (!unitIsSpace(bytes, encoding, afterName) && !unitIs(bytes, encoding, afterName, '/') &&
            !unitIs(bytes, encoding, afterName, '>')) {
            throw std::runtime_error("parsex: unexpected markup while seeking <" + tagName +
                                     ">");
        }
        // End of start tag: the first '>' outside quotes ('>' may appear raw
        // inside attribute values; '/' only ends the tag as '/>').
        std::size_t tagPos = afterName;
        while (tagPos < bytes.size()) {
            const int code = unitAscii(bytes, encoding, tagPos);
            if (code == '"' || code == '\'') {
                tagPos = skipQuoted(bytes, encoding, tagPos);
                continue;
            }
            if (code == '>') {
                return {.openPos = scanPos, .tagEndPos = tagPos + width, .selfClosed = false};
            }
            if (code == '/' && unitIs(bytes, encoding, tagPos + width, '>')) {
                return {.openPos = scanPos, .tagEndPos = tagPos + (2 * width), .selfClosed = true};
            }
            tagPos += width;
        }
        throw std::runtime_error("parsex: unterminated start tag <" + tagName + ">");
    }
    throw std::runtime_error("parsex: end of input while seeking <" + tagName + ">");
}

// Scans forward from cursor for `</tagName>` (optional whitespace before
// '>'), with the same skipping and fail-loud rules as scanStartTag.
std::size_t scanEndTag(const std::string& bytes, RawEncodingClass encoding, std::size_t cursor,
                       const std::string& tagName) {
    requireAsciiName(tagName);
    const std::size_t width = unitWidth(encoding);
    std::size_t scanPos = cursor;
    while (scanPos < bytes.size()) {
        if (!unitIs(bytes, encoding, scanPos, '<')) {
            scanPos += width;
            continue;
        }
        std::size_t skippedPos = 0;
        if (skipSpecialUnit(bytes, encoding, scanPos, skippedPos)) {
            scanPos = skippedPos;
            continue;
        }
        const std::size_t namePos = scanPos + width;
        if (unitIs(bytes, encoding, namePos, '/')) {
            const std::size_t afterSlash = namePos + width;
            if (matchLiteral(bytes, encoding, afterSlash, tagName)) {
                std::size_t tagPos = afterSlash + (tagName.size() * width);
                while (tagPos < bytes.size() && unitIsSpace(bytes, encoding, tagPos)) {
                    tagPos += width;
                }
                if (unitIs(bytes, encoding, tagPos, '>')) {
                    return tagPos + width;
                }
            }
        }
        throw std::runtime_error("parsex: unexpected markup while seeking </" + tagName + ">");
    }
    throw std::runtime_error("parsex: end of input while seeking </" + tagName + ">");
}

// 1-based line number of a byte offset, counted from the raw bytes so it
// agrees with the span by construction. Counts '\n' units (matches libxml2's
// reported lines for LF and CRLF files; lone-CR files would undercount —
// vanishingly rare, accepted).
std::size_t lineNumberAt(const std::string& bytes, RawEncodingClass encoding,
                         std::size_t offset) {
    std::size_t breaks = 0;
    const std::size_t scanEnd = std::min(offset, bytes.size());
    if (encoding == RawEncodingClass::SingleByte) {
        for (std::size_t idx = 0; idx < scanEnd; ++idx) {
            if (byteAt(bytes, idx) == 0x0A) {
                ++breaks;
            }
        }
    } else {
        for (std::size_t idx = 0; idx + 1 < scanEnd; idx += 2) {
            const bool isBreak =
                (encoding == RawEncodingClass::Utf16Le)
                    ? (byteAt(bytes, idx) == 0x0A && byteAt(bytes, idx + 1) == 0x00)
                    : (byteAt(bytes, idx) == 0x00 && byteAt(bytes, idx + 1) == 0x0A);
            if (isBreak) {
                ++breaks;
            }
        }
    }
    return breaks + 1;
}

// libxml2's xmlChar strings are unsigned-char based; char-based std::string
// needs the cast at the boundary (NOLINT per codebase practice for
// libxml2-mandated signatures, cf. schema_registry.cpp).
std::string fromXmlStr(const xmlChar* text) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): xmlChar is libxml2's unsigned-char string type.
    return reinterpret_cast<const char*>(text);
}

std::string qualifiedName(const xmlChar* prefix, const xmlChar* localName) {
    std::string name = fromXmlStr(localName);
    if (prefix != nullptr) {
        name = fromXmlStr(prefix) + ":" + name;
    }
    return name;
}

struct ParserCtxtDeleter {
    void operator()(xmlParserCtxt* ctxt) const noexcept {
        if (ctxt != nullptr) {
            xmlFreeParserCtxt(ctxt);
        }
    }
};
using ParserCtxtPtr = std::unique_ptr<xmlParserCtxt, ParserCtxtDeleter>;

struct LoaderState {
    const std::string* fileBytes = nullptr;
    RawEncodingClass encoding = RawEncodingClass::SingleByte;
    std::size_t cursor = 0;       // raw scan position: always past the last consumed token
    std::vector<RawNode*> stack;  // current parent chain (non-owning)
    std::unique_ptr<RawNode> root;
};

// SAX2 packs each attribute as 5 slots: local, prefix, URI, valueStart,
// valueEnd (values arrive UTF-8 transcoded; external entities are never
// loaded, see loader.hpp). Slot indexing is mandated by libxml2's
// startElementNsSAX2Func signature.
constexpr std::size_t kAttrSlots = 5;

const xmlChar* attrSlot(const xmlChar** slots, std::size_t attrIdx, std::size_t slot) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): SAX2 attribute slot layout mandated by libxml2.
    return slots[(attrIdx * kAttrSlots) + slot];
}

void onStartElement(void* userData, const xmlChar* localName, const xmlChar* prefix,
                    const xmlChar* /*uri*/, int nbNamespaces, const xmlChar** namespaces,
                    int numAttrs, int /*numDefaulted*/, const xmlChar** attributes) {
    auto* state = static_cast<LoaderState*>(userData);

    auto node = std::make_unique<RawNode>();
    node->tagName = qualifiedName(prefix, localName);
    for (int attrIdx = 0; attrIdx < numAttrs; ++attrIdx) {
        const auto base = static_cast<std::size_t>(attrIdx);
        const auto* first =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): xmlChar value bounds from libxml2.
            reinterpret_cast<const char*>(attrSlot(attributes, base, 3));
        const auto* last =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): xmlChar value bounds from libxml2.
            reinterpret_cast<const char*>(attrSlot(attributes, base, 4));
        node->attributes.emplace_back(qualifiedName(attrSlot(attributes, base, 1),
                                                    attrSlot(attributes, base, 0)),
                                      std::string(first, last));
    }
    // Namespace declarations are attributes in the raw tag text, so they join
    // the list (after element attributes; source interleaving is unknowable).
    for (int nsIdx = 0; nsIdx < nbNamespaces; ++nsIdx) {
        const auto base = static_cast<std::size_t>(nsIdx);
        std::string attrName = "xmlns";
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): SAX2 namespace pair layout mandated by libxml2.
        const xmlChar* nsPrefix = namespaces[(base * 2)];
        if (nsPrefix != nullptr) {
            attrName += ":" + fromXmlStr(nsPrefix);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): SAX2 namespace pair layout mandated by libxml2.
        const xmlChar* href = namespaces[(base * 2) + 1];
        node->attributes.emplace_back(std::move(attrName),
                                      href != nullptr ? fromXmlStr(href) : "");
    }

    const StartTagBounds bounds =
        scanStartTag(*state->fileBytes, state->encoding, state->cursor, node->tagName);
    state->cursor = bounds.tagEndPos;
    node->span.startOffset = bounds.openPos;
    node->span.lineNumber =
        lineNumberAt(*state->fileBytes, state->encoding, node->span.startOffset);
    // A self-closed tag completes its span immediately; otherwise the end tag
    // scan (below) finishes it. endOffset == startOffset therefore always
    // means "end not yet seen".
    node->span.endOffset = bounds.selfClosed ? bounds.tagEndPos : bounds.openPos;

    RawNode* entered = nullptr;
    if (state->stack.empty()) {
        // Document element. A second top-level element is ill-formed input:
        // its subtree is skipped here and the document is rejected via the
        // wellFormed check after parsing.
        if (state->root == nullptr) {
            state->root = std::move(node);
            entered = state->root.get();
        }
    } else {
        node->parent = state->stack.back();
        state->stack.back()->children.push_back(std::move(node));
        entered = state->stack.back()->children.back().get();
    }
    if (entered != nullptr) {
        state->stack.push_back(entered);
    }
}

void onEndElement(void* userData, const xmlChar* /*localName*/, const xmlChar* /*prefix*/,
                  const xmlChar* /*uri*/) {
    auto* state = static_cast<LoaderState*>(userData);
    // An empty stack means the matching start belonged to a skipped subtree
    // (second document element); the wellFormed check rejects the document.
    if (state->stack.empty()) {
        return;
    }
    RawNode* node = state->stack.back();
    state->stack.pop_back();
    if (node->span.endOffset == node->span.startOffset) {
        node->span.endOffset =
            scanEndTag(*state->fileBytes, state->encoding, state->cursor, node->tagName);
        state->cursor = node->span.endOffset;
    }
}

// Re-points every child's parent link at its actual owner. Needed once after
// moving the finished tree: RawNode has compiler-generated move semantics,
// so moving a node orphans its children's parent pointers (they still point
// at the moved-from object).
void relinkParents(RawNode& node) {
    for (auto& child : node.children) {
        child->parent = &node;
        relinkParents(*child);
    }
}

}  // namespace

RawDocument loadRawDocument(const std::filesystem::path& path) {
    const std::string bytes = readFileBytes(path);

    LoaderState state;
    state.fileBytes = &bytes;
    state.encoding = detectEncodingClass(bytes);

    // Diagnostic-only label for libxml2 messages (not a file open — the bytes
    // are already in memory). path::c_str() is wchar_t on Windows, so copy
    // the u8string() code units byte-for-byte.
    const auto utf8Path = path.u8string();
    const std::string narrowPath{utf8Path.begin(), utf8Path.end()};

    ParserCtxtPtr ctxt(xmlNewParserCtxt());
    if (ctxt == nullptr) {
        throw std::runtime_error("parsex: cannot create parser context for: " + narrowPath);
    }
    xmlSAXHandler sax{};
    sax.startElementNs = &onStartElement;
    sax.endElementNs = &onEndElement;
    sax.initialized = XML_SAX2_MAGIC;
    // Copy into libxml2's own struct: re-pointing ctxt->sax at stack memory
    // makes xmlFreeParserCtxt() free it (SIGABRT at cleanup, seen in the spike).
    *ctxt->sax = sax;
    ctxt->userData = &state;

    // Single parse pass over the bytes already in memory (the buffer outlives
    // the parse; it is a local). SAX callbacks drive the raw scanner in
    // document order. Callbacks may throw on logic errors; the unique_ptr
    // above still frees the context during unwinding.
    xmlDocPtr doc = xmlCtxtReadMemory(ctxt.get(), bytes.data(),
                                      static_cast<int>(bytes.size()), narrowPath.c_str(),
                                      nullptr, XML_PARSE_NONET);
    xmlFreeDoc(doc);  // null with a custom SAX handler; freed defensively
    if (ctxt->wellFormed != 1) {
        throw std::runtime_error("parsex: ill-formed XML in: " + narrowPath);
    }
    if (state.root == nullptr) {
        throw std::runtime_error("parsex: no document element in: " + narrowPath);
    }

    RawDocument document{XmlDocPtr(nullptr)};
    document.root = std::move(*state.root);
    relinkParents(document.root);
    return document;
}

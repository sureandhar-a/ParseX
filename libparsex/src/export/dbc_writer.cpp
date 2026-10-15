#include <parsex/export/dbc_writer.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace {

constexpr std::uint32_t kExtendedFlag = 0x80000000U;
constexpr std::string_view kNoNode = "Vector__XXX";

// Vector's canonical NS_ list; tools match the keywords, not the order.
constexpr std::array kNewSymbols = {
    "NS_DESC_",      "CM_",         "BA_DEF_",       "BA_",         "VAL_",          "CAT_DEF_",
    "CAT_",          "FILTER",      "BA_DEF_DEF_",   "EV_DATA_",    "ENVVAR_DATA_",  "SGTYPE_",
    "SGTYPE_VAL_",   "BA_DEF_SGTYPE_", "BA_SGTYPE_", "SIG_TYPE_REF_", "VAL_TABLE_", "SIG_GROUP_",
    "SIG_VALTYPE_",  "SIGTYPE_VALTYPE_", "BO_TX_BU_", "BA_DEF_REL_", "BA_REL_",     "BA_DEF_DEF_REL_",
    "BU_SG_REL_",    "BU_EV_REL_",  "BU_BO_REL_",    "SG_MUL_VAL_",
};

// std::to_chars is locale-free and yields the shortest string that round-trips.
std::string num(double value) {
    std::array<char, 40> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return {buffer.data(), result.ptr};
}

// Names go into whitespace-delimited DBC tokens; anything else would corrupt the file.
std::string ident(const std::string& name) {
    std::string out = name;
    for (char& c : out) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok) {
            c = '_';
        }
    }
    if (out.empty() || (out.front() >= '0' && out.front() <= '9')) {
        out.insert(out.begin(), '_');
    }
    return out;
}

// Inside a quoted DBC string only '"' and '\' need protecting.
std::string quoted(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    out += '"';
    return out;
}

std::string joinIdents(const std::vector<std::string>& names, char separator) {
    std::string out;
    for (const auto& name : names) {
        if (!out.empty()) {
            out += separator;
        }
        out += ident(name);
    }
    return out.empty() ? std::string(kNoNode) : out;
}

std::uint32_t dbcId(const ExportFrame& frame) {
    return frame.extended ? (frame.id | kExtendedFlag) : frame.id;
}

// VFrameFormat enum index: 0 std, 1 ext, 14 FD std, 15 FD ext.
int frameFormat(const ExportFrame& frame) {
    if (frame.canFd) {
        return frame.extended ? 15 : 14;
    }
    return frame.extended ? 1 : 0;
}

void appendSignal(std::string& out, const ExportSignal& signal) {
    out += " SG_ " + ident(signal.name) + " : " + std::to_string(signal.startBit) + "|" +
           std::to_string(signal.bitLength) + "@";
    out += signal.byteOrder == ByteOrder::LeastSignificantByteFirst ? '1' : '0';
    out += signal.isSigned ? '-' : '+';
    out += " (" + num(signal.factor) + "," + num(signal.offset) + ") [" + num(signal.min.value_or(0.0)) + "|" +
           num(signal.max.value_or(0.0)) + "] " + quoted(signal.unit) + " " + joinIdents(signal.receivers, ',') + "\n";
}

void appendFdAttributes(std::string& out, const ExportDatabase& database) {
    out += "BA_DEF_ BO_ \"VFrameFormat\" ENUM \"StandardCAN\",\"ExtendedCAN\"";
    for (int i = 0; i < 12; ++i) {
        out += ",\"reserved\"";
    }
    out += ",\"StandardCAN_FD\",\"ExtendedCAN_FD\";\n";
    out += "BA_DEF_ \"BusType\" STRING ;\n";
    out += "BA_DEF_DEF_ \"VFrameFormat\" \"StandardCAN\";\n";
    out += "BA_DEF_DEF_ \"BusType\" \"CAN\";\n";
    out += "BA_ \"BusType\" \"CAN FD\";\n";
    for (const auto& frame : database.frames) {
        if (const int format = frameFormat(frame); format != 0) {
            out += "BA_ \"VFrameFormat\" BO_ " + std::to_string(dbcId(frame)) + " " + std::to_string(format) + ";\n";
        }
    }
    out += "\n";
}

}  // namespace

std::string toDbc(const ExportDatabase& database) {
    std::string out = "VERSION \"\"\n\nNS_ :\n";
    for (const auto* keyword : kNewSymbols) {
        out += std::string("\t") + keyword + "\n";
    }
    out += "\nBS_:\n\nBU_:";
    for (const auto& node : database.nodes) {
        out += " " + ident(node);
    }
    out += "\n\n";

    for (const auto& frame : database.frames) {
        const std::string transmitter =
            frame.transmitters.empty() ? std::string(kNoNode) : ident(frame.transmitters.front());
        out += "BO_ " + std::to_string(dbcId(frame)) + " " + ident(frame.name) + ": " + std::to_string(frame.length) +
               " " + transmitter + "\n";
        for (const auto& signal : frame.signals) {
            appendSignal(out, signal);
        }
        out += "\n";
    }

    bool anyFd = false;
    bool anyMultiTx = false;
    for (const auto& frame : database.frames) {
        anyFd = anyFd || frame.canFd;
        if (frame.transmitters.size() > 1) {
            anyMultiTx = true;
            out += "BO_TX_BU_ " + std::to_string(dbcId(frame)) + " : " + joinIdents(frame.transmitters, ',') + ";\n";
        }
    }
    if (anyMultiTx) {
        out += "\n";
    }
    if (anyFd) {
        appendFdAttributes(out, database);
    }

    for (const auto& frame : database.frames) {
        for (const auto& signal : frame.signals) {
            if (signal.valueTable.empty()) {
                continue;
            }
            out += "VAL_ " + std::to_string(dbcId(frame)) + " " + ident(signal.name);
            for (const auto& entry : signal.valueTable) {
                out += " " + std::to_string(entry.value) + " " + quoted(entry.label);
            }
            out += " ;\n";
        }
    }
    return out;
}

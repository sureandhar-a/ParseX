#include <parsex/export/csv_writer.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kHeader =
    "frame,frame_id,extended,can_fd,length,transmitters,signal,start_bit,bit_length,byte_order,signed,"
    "factor,offset,min,max,unit,init_value,receivers,value_table";

// Number of cells in a row; a frame without signals fills the signal columns with blanks.
constexpr std::size_t kSignalColumns = 13;

// RFC 4180: quote only when the field would otherwise break the row, and double embedded quotes.
void appendField(std::string& out, std::string_view field) {
    if (field.find_first_of(",\"\r\n") == std::string_view::npos) {
        out += field;
        return;
    }
    out += '"';
    for (char c : field) {
        if (c == '"') {
            out += '"';
        }
        out += c;
    }
    out += '"';
}

// std::to_chars gives the shortest round-trip form and never consults the locale, unlike ostream or printf.
std::string formatNumber(double value) {
    char buf[64];
    const auto result = std::to_chars(buf, buf + sizeof(buf), value);
    return std::string(buf, result.ptr);
}

std::string formatOptional(const std::optional<double>& value) {
    return value ? formatNumber(*value) : std::string{};
}

std::string joinSemicolon(const std::vector<std::string>& items) {
    std::string out;
    for (const auto& item : items) {
        if (!out.empty()) {
            out += ';';
        }
        out += item;
    }
    return out;
}

std::string formatValueTable(const std::vector<ValueTableEntry>& table) {
    std::string out;
    for (const auto& entry : table) {
        if (!out.empty()) {
            out += ';';
        }
        out += std::format("{}={}", entry.value, entry.label);
    }
    return out;
}

// Appends the cells as one CRLF-terminated row. CRLF is the RFC 4180 record terminator, including the last row.
void appendRow(std::string& out, const std::vector<std::string>& cells) {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i != 0) {
            out += ',';
        }
        appendField(out, cells[i]);
    }
    out += "\r\n";
}

}  // namespace

std::string toCsv(const ExportDatabase& database) {
    std::string out;
    out += kHeader;
    out += "\r\n";

    for (const auto& frame : database.frames) {
        std::vector<std::string> row{
            frame.name,
            std::format("0x{:X}", frame.id),
            frame.extended ? "true" : "false",
            frame.canFd ? "true" : "false",
            std::format("{}", frame.length),
            joinSemicolon(frame.transmitters),
        };

        if (frame.signals.empty()) {
            row.resize(row.size() + kSignalColumns);
            appendRow(out, row);
            continue;
        }

        for (const auto& signal : frame.signals) {
            std::vector<std::string> cells = row;
            cells.push_back(signal.name);
            cells.push_back(std::format("{}", signal.startBit));
            cells.push_back(std::format("{}", signal.bitLength));
            cells.emplace_back(signal.byteOrder == ByteOrder::MostSignificantByteFirst ? "big_endian"
                                                                                       : "little_endian");
            cells.emplace_back(signal.isSigned ? "true" : "false");
            cells.push_back(formatNumber(signal.factor));
            cells.push_back(formatNumber(signal.offset));
            cells.push_back(formatOptional(signal.min));
            cells.push_back(formatOptional(signal.max));
            cells.push_back(signal.unit);
            cells.push_back(formatOptional(signal.initValue));
            cells.push_back(joinSemicolon(signal.receivers));
            cells.push_back(formatValueTable(signal.valueTable));
            appendRow(out, cells);
        }
    }
    return out;
}

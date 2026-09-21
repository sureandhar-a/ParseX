#include <parsex/validator/validator.hpp>

#include <array>

// CAN FD DLC table: non-linear above DLC 8. Fixed lookup, never a formula.
std::optional<std::uint32_t> Validator::canFdBytesForDlc(int dlc) {
    static constexpr std::array<std::uint32_t, 16> kTable = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64,
    };
    if (dlc < 0 || dlc > 15) {
        return std::nullopt;
    }
    return kTable[static_cast<std::size_t>(dlc)];
}

bool Validator::isValidClassicCanLength(std::uint32_t length) {
    return length <= 8;
}

bool Validator::isValidCanFdLength(std::uint32_t length) {
    switch (length) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 12:
        case 16:
        case 20:
        case 24:
        case 32:
        case 48:
        case 64:
            return true;
        default:
            return false;
    }
}

namespace {

void checkPayloadLength(const std::string& kind, const std::string& name,
                        std::uint32_t length, RawSpan span, ValidationResult& out) {
    // No FD flag exists on Frame/Pdu: lengths 0..8 are valid for both,
    // extended FD lengths pass as FD, anything else fails.
    if (Validator::isValidClassicCanLength(length) ||
        Validator::isValidCanFdLength(length)) {
        return;
    }
    ValidationError finding;
    finding.severity = Severity::Error;
    finding.code = "can.dlc_mismatch";
    finding.message = kind + " '" + name + "' declares payload length " +
                      std::to_string(length) + " bytes, which is neither valid classic CAN " +
                      "(0..8) nor valid CAN FD (0..8, 12, 16, 20, 24, 32, 48, 64)";
    finding.location = span;
    out.errors.push_back(std::move(finding));
}

}  // namespace

// PAR-108: DLC part only; signal-overlap joins in PAR-110.
ValidationResult Validator::validateCanSemantics(const ParsedProject& project) const {
    ValidationResult result;
    for (const auto& file : project.files) {
        for (const auto& frame : file.frames) {
            checkPayloadLength("Frame", frame.common.shortName, frame.length,
                               frame.common.rawSpanRef, result);
        }
        for (const auto& pdu : file.pdus) {
            checkPayloadLength("Pdu", pdu.common.shortName, pdu.length,
                               pdu.common.rawSpanRef, result);
        }
    }
    return result;
}

#include <parsex/validator/validator.hpp>

#include <array>
#include <vector>

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

// Intel/little-endian: LSB-first ascending — occupies [start, start+len).
std::vector<int> Validator::physicalBitsForIntelSignal(int startBit, int bitLength) {
    std::vector<int> bits;
    if (startBit < 0 || bitLength <= 0) {
        return bits;
    }
    bits.reserve(static_cast<std::size_t>(bitLength));
    for (int idx = 0; idx < bitLength; ++idx) {
        bits.push_back(startBit + idx);
    }
    return bits;
}

// Motorola/big-endian: LSB0-within-byte but big-endian-overall (bit 7 = MSB
// of byte 0, byte 1 continues at 15..8). startBit names the MSB; the signal
// extends toward the LSB, wrapping to the next byte's MSB at byte edges.
std::vector<int> Validator::physicalBitsForMotorolaSignal(int startBit, int bitLength) {
    std::vector<int> bits;
    if (startBit < 0 || bitLength <= 0) {
        return bits;
    }
    bits.reserve(static_cast<std::size_t>(bitLength));
    int pos = startBit;
    for (int idx = 0; idx < bitLength; ++idx) {
        bits.push_back(pos);
        if (pos % 8 == 0) {
            pos += 15;  // LSB of byte N -> MSB of byte N+1
        } else {
            --pos;
        }
    }
    return bits;
}

std::vector<int> Validator::physicalBitsForSignal(ByteOrder byteOrder, int startBit,
                                                  int bitLength) {
    if (byteOrder == ByteOrder::LeastSignificantByteFirst) {
        return physicalBitsForIntelSignal(startBit, bitLength);
    }
    return physicalBitsForMotorolaSignal(startBit, bitLength);
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

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <parsex/model/parsed_file.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>

// Flat, export-ready view of one file's CAN network. Every join the domain
// model leaves open (CAN IDs on CAN-FRAME-TRIGGERINGs, packing on PDU
// mappings, scaling/limits/units behind COMPU-METHOD and UNIT refs, senders
// behind FRAME-PORTs) is resolved here, once, so the DBC and CSV writers are
// pure formatters over this struct.

struct ExportSignal {
    std::string name;
    // Frame-relative start bit in AUTOSAR/DBC numbering (byte * 8 + bit,
    // bit 0 = LSB of the byte): the LSB for little endian, the MSB for big
    // endian. Equals the PDU mapping START-POSITION plus the PDU's offset in
    // the frame, so DBC can take it verbatim.
    std::uint32_t startBit = 0;
    std::uint32_t bitLength = 0;
    ByteOrder byteOrder = ByteOrder::LeastSignificantByteFirst;
    bool isSigned = false;
    double factor = 1.0;
    double offset = 0.0;
    std::optional<double> min;  // physical units
    std::optional<double> max;  // physical units
    std::string unit;           // "" when none
    std::optional<double> initValue;  // raw value
    std::vector<std::string> receivers;  // ECU short names
    std::vector<ValueTableEntry> valueTable;
};

struct ExportFrame {
    std::string name;
    std::uint32_t id = 0;  // 11- or 29-bit identifier, never carries a flag bit
    bool extended = false;
    bool canFd = false;
    std::uint32_t length = 0;  // bytes
    std::vector<std::string> transmitters;  // ECU short names
    std::vector<ExportSignal> signals;
};

struct ExportDatabase {
    std::string busName;  // first cluster's short name, "" when none
    std::optional<std::uint32_t> baudrate;
    std::vector<std::string> nodes;  // ECU short names, document order
    std::vector<ExportFrame> frames;  // document order
    std::vector<Warning> warnings;    // what could not be resolved, and why
};

ExportDatabase buildExportDatabase(const ParsedFile& file);

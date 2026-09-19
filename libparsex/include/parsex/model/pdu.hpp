#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <parsex/model/common_fields.hpp>

enum class ByteOrder { MostSignificantByteFirst, LeastSignificantByteFirst };

struct PduSignalMapping {
    std::string signalShortNameRef;
    std::uint32_t startPosition = 0;
    ByteOrder byteOrder = ByteOrder::MostSignificantByteFirst;
};

struct Pdu {
    CommonFields common;
    std::uint32_t length = 0;
    std::vector<PduSignalMapping> signalMappings;
};
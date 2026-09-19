#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <parsex/model/common_fields.hpp>
#include <parsex/model/pdu.hpp>

struct ValueTableEntry {
    std::int64_t value = 0;
    std::string label;
};

struct Signal {
    CommonFields common;
    std::uint32_t startBit = 0;
    std::uint32_t bitLength = 0;
    ByteOrder byteOrder = ByteOrder::MostSignificantByteFirst;
    bool isSigned = false;  // renamed from 'signed' (C++ keyword)
    double factor = 1.0;
    double offset = 0.0;
    std::optional<double> min;
    std::optional<double> max;
    std::optional<std::string> unit;
    std::optional<double> initValue;
    std::vector<std::string> receivers;
    std::optional<std::vector<ValueTableEntry>> valueTable;
};
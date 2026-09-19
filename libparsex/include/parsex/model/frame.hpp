#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <parsex/model/common_fields.hpp>

struct FramePduMapping {
    std::string pduShortNameRef;
    std::uint32_t startPosition = 0;
};

struct Frame {
    CommonFields common;
    std::uint32_t length = 0;
    std::vector<std::string> transmitters;
    std::vector<FramePduMapping> pdus;
};
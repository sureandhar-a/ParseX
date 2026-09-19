#pragma once

// composition over inheritance
#include "common_fields.hpp"
#include <vector>
#include <optional>
#include <cstdint>

struct Cluster {
    CommonFields common;
    std::optional<std::uint32_t> baudrate;
    std::vector<std::string> physicalChannels;
};
#pragma once

// composition over inheritance
#include "common_fields.hpp"
#include <vector>
#include <optional>
#include <cstdint>

struct EcuInstance {
    CommonFields common;
    std::vector<std::string> connectedChannels;
    std::vector<std::string> controllers;
};
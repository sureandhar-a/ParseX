#pragma once

#include <string>
#include <vector>
#include <parsex/model/common_fields.hpp>

struct SignalGroup {
    CommonFields common;
    std::vector<std::string> members;
};
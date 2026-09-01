#pragma once

#include <variant>
#include <parsex/model/can_extension.hpp>

using ProtocolSpecific = std::variant<std::monostate, CanExtension>;
inline ProtocolSpecific protocolSpecific;

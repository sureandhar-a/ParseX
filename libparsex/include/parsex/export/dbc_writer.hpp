#pragma once

#include <string>

#include <parsex/export/can_db.hpp>

// Vector DBC text for an ExportDatabase. Deterministic, locale-independent.
std::string toDbc(const ExportDatabase& database);

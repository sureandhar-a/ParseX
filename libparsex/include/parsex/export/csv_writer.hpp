#pragma once

#include <string>

#include <parsex/export/can_db.hpp>

// RFC 4180 CSV, one row per signal. Deterministic, locale-independent.
std::string toCsv(const ExportDatabase& database);

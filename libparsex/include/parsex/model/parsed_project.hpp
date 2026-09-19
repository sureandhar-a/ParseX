#pragma once

#include <map>
#include <string>
#include <vector>

#include <parsex/model/parsed_file.hpp>

struct ResolvedReference {
    std::string targetShortNamePath;
};

struct ParsedProject {
    std::vector<ParsedFile> files;
    std::map<std::string, ResolvedReference> resolvedRefs;
};

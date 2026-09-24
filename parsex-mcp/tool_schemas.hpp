#pragma once

#include <nlohmann/json.hpp>

// JSON Schemas describing each tool's arguments and results.
// Draft 2020-12 by default (no $schema override needed).

inline nlohmann::json parseInputSchema() {
    return {{"type", "object"},
            {"properties", {{"path", {{"type", "string"}, {"description", "Input ARXML file path"}}}}},
            {"required", nlohmann::json::array({"path"})},
            {"additionalProperties", false}};
}

inline nlohmann::json parseOutputSchema() {
    return {{"type", "object"},
            {"properties",
             {{"sourcePath", {{"type", "string"}}},
              {"autosarRelease", {{"type", "string"}}},
              {"counts", {{"type", "object"}}},
              {"warnings", {{"type", "array"}}}}},
            {"required", nlohmann::json::array({"sourcePath"})},
            {"additionalProperties", false}};
}

inline nlohmann::json validateInputSchema() {
    nlohmann::json strict;
    strict["type"] = "boolean";
    strict["description"] = "Strict mode: warnings fail the verdict";
    strict["default"] = false;
    nlohmann::json path;
    path["type"] = "string";
    path["description"] = "Input ARXML file path";
    return {{"type", "object"},
            {"properties", {{"path", path}, {"strict", strict}}},
            {"required", nlohmann::json::array({"path"})},
            {"additionalProperties", false}};
}

inline nlohmann::json validateOutputSchema() {
    return {{"type", "object"},
            {"properties",
             {{"passed", {{"type", "boolean"}}}, {"errors", {{"type", "array"}}}}},
            {"required", nlohmann::json::array({"passed", "errors"})},
            {"additionalProperties", false}};
}

inline nlohmann::json diffInputSchema() {
    return {{"type", "object"},
            {"properties",
             {{"basePath", {{"type", "string"}, {"description", "Base ARXML file"}}},
              {"targetPath", {{"type", "string"}, {"description", "Target ARXML file"}}}}},
            {"required", nlohmann::json::array({"basePath", "targetPath"})},
            {"additionalProperties", false}};
}

inline nlohmann::json diffOutputSchema() {
    return {{"type", "object"},
            {"properties",
             {{"entries", {{"type", "array"}}}, {"diagnostics", {{"type", "array"}}}}},
            {"required", nlohmann::json::array({"entries"})},
            {"additionalProperties", false}};
}

inline nlohmann::json writeInputSchema() {
    nlohmann::json path;
    path["type"] = "string";
    path["description"] = "Input ARXML file path";
    nlohmann::json outputPath;
    outputPath["type"] = "string";
    outputPath["description"] = "Output ARXML file path";
    nlohmann::json apply;
    apply["type"] = "boolean";
    apply["description"] = "Write to disk; otherwise preview only";
    apply["default"] = false;
    return {{"type", "object"},
            {"properties", {{"path", path}, {"outputPath", outputPath}, {"apply", apply}}},
            {"required", nlohmann::json::array({"path", "outputPath"})},
            {"additionalProperties", false}};
}

inline nlohmann::json writeOutputSchema() {
    return {{"type", "object"},
            {"properties",
             {{"input", {{"type", "string"}}},
              {"output", {{"type", "string"}}},
              {"applied", {{"type", "boolean"}}},
              {"success", {{"type", "boolean"}}}}},
            {"required", nlohmann::json::array({"input", "output", "applied", "success"})},
            {"additionalProperties", false}};
}

#include <parsex/json_contract/schema_validate.hpp>

#include <fstream>
#include <sstream>
#include <vector>

#include <nlohmann/json-schema.hpp>

namespace parsex::json_contract {
namespace {

nlohmann::json loadJsonFile(const std::filesystem::path& path, std::string* errorOut) {
    std::ifstream schemaFile(path);
    if (!schemaFile.good()) {
        throw std::runtime_error("cannot open schema file: " + path.string());
    }
    std::ostringstream raw;
    raw << schemaFile.rdbuf();
    try {
        return nlohmann::json::parse(raw.str());
    } catch (const std::exception& ex) {
        throw std::runtime_error("cannot parse schema file " + path.string() + ": " + ex.what());
    }
}

// Collecting handler so failures carry the validator's own diagnostics.
class CollectingHandler : public nlohmann::json_schema::error_handler {
   public:
    void error(const nlohmann::json::json_pointer& ptr, const nlohmann::json& instance,
               const std::string& message) override {
        failed_ = true;
        std::ostringstream detail;
        detail << "at " << ptr.to_string() << ": " << message << " (instance: " << instance.dump()
               << ")";
        messages_.push_back(detail.str());
    }

    [[nodiscard]] bool failed() const { return failed_; }
    [[nodiscard]] std::string joined() const {
        std::ostringstream out;
        for (std::size_t i = 0; i < messages_.size(); ++i) {
            if (i != 0) {
                out << "\n";
            }
            out << messages_[i];
        }
        return out.str();
    }

   private:
    bool failed_ = false;
    std::vector<std::string> messages_;
};

// Draft 2020-12 "format": "uri-reference" (used by the envelope schema) is
// not implemented by this library's default checker — pass it through and
// delegate everything else.
void parsexFormatChecker(const std::string& format, const std::string& value) {
    if (format == "uri-reference") {
        return;
    }
    nlohmann::json_schema::default_string_format_check(format, value);
}

}  // namespace

bool validatesAgainstSchema(const nlohmann::json& document,
                            const std::filesystem::path& schemaPath, std::string* errorOut) {
    auto fail = [&](const std::string& msg) {
        if (errorOut != nullptr) {
            *errorOut = msg;
        }
        return false;
    };

    nlohmann::json schema = nullptr;
    try {
        schema = loadJsonFile(schemaPath, errorOut);
    } catch (const std::exception& ex) {
        return fail(ex.what());
    }

    // Resolve the envelope's relative $refs from the schema file's directory.
    const std::filesystem::path baseDir = schemaPath.parent_path();
    auto loader = [&](const nlohmann::json_uri& uri, nlohmann::json& value) {
        const std::string uriString = uri.to_string();
        // Match by filename suffix so both relative refs
        // ("validation_result.schema.json") and $id URLs
        // ("https://parsex.dev/schemas/v1/validation_result.json") resolve.
        for (const char* file : {"validation_result.schema.json", "diff_report.schema.json",
                                 "telemetry_report.schema.json", "envelope.schema.json"}) {
            if (uriString.find(file) != std::string::npos) {
                try {
                    value = loadJsonFile(baseDir / file, errorOut);
                } catch (const std::exception& ex) {
                    throw std::runtime_error(ex.what());
                }
                return;
            }
        }
        throw std::runtime_error("schemaLoader: unrecognized ref " + uriString);
    };

    try {
        nlohmann::json_schema::json_validator validator(loader, parsexFormatChecker);
        validator.set_root_schema(schema);
        CollectingHandler handler;
        validator.validate(document, handler);
        if (handler.failed()) {
            return fail(handler.joined());
        }
        return true;
    } catch (const std::exception& ex) {
        return fail(std::string("validator exception: ") + ex.what());
    }
}

}  // namespace parsex::json_contract

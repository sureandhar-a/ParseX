#include <parsex/json_contract/envelope.hpp>

#include <string>

#include <parsex/json_contract/version.hpp>
#include <parsex/version.hpp>

namespace parsex::json_contract {

nlohmann::json wrapEnvelope(std::string_view kind, nlohmann::json payload) {
    nlohmann::json envelope;
    envelope["$schema"] = "https://parsex.dev/schemas/v1/envelope.json";
    envelope["contractVersion"] = std::string(kContractVersion);
    envelope["toolVersion"] = std::string(libparsexVersion());
    envelope["kind"] = std::string(kind);
    envelope["payload"] = std::move(payload);
    return envelope;
}

}  // namespace parsex::json_contract

#include <parsex/version.hpp>

#include <string_view>

#include <parsex/version_config.hpp>

std::string_view libparsexVersion() {
    return PARSEX_VERSION;
}

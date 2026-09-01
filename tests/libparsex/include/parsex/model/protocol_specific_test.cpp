#include <gtest/gtest.h>
#include <parsex/model/protocol_specific.hpp>
#include <parsex/model/can_extension.hpp>
#include <type_traits>
#include <variant>

TEST(ProtoconSpeficTest, defaultConstructor)
{
    ProtocolSpecific protocolSpecific{};

    EXPECT_TRUE(std::holds_alternative<std::monostate>(protocolSpecific));
    EXPECT_NE(std::get_if<std::monostate>(&protocolSpecific), nullptr);
    EXPECT_EQ(std::get_if<CanExtension>(&protocolSpecific), nullptr);

    std::visit([](auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            EXPECT_TRUE(true);
        } else if constexpr (std::is_same_v<T, CanExtension>) {
            GTEST_FAIL() << "default-constructed ProtocolSpecific should not hold CanExtension";
        }
    }, protocolSpecific);
}

TEST(ProtoconSpeficTest, ProtocolSpecificWithCANExtension)
{
    ProtocolSpecific protocolSpecific = CanExtension{};

    EXPECT_TRUE(std::holds_alternative<CanExtension>(protocolSpecific));
    EXPECT_EQ(std::get_if<std::monostate>(&protocolSpecific), nullptr);
    EXPECT_NE(std::get_if<CanExtension>(&protocolSpecific), nullptr);

    std::visit([](auto&& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            GTEST_FAIL() << "CanExtension variant should not hold std::monostate";
        } else if constexpr (std::is_same_v<T, CanExtension>) {
            EXPECT_TRUE(true);
        }
    }, protocolSpecific);
}

TEST(ProtoconSpeficTest, badVariantAccessThrowsForWrongAlternative)
{
    ProtocolSpecific protocolSpecific{};

    bool threw = false;
    try {
        (void)std::get<CanExtension>(protocolSpecific);
    } catch (const std::bad_variant_access&) {
        threw = true;
    }

    EXPECT_TRUE(threw);
}
#include <gtest/gtest.h>

#include "../../parsex-cli/color.hpp"

using parsex::cli::colorEnabled;

// TTY + no flag + no env -> color on.
TEST(ColorEnabled, TtyWithoutFlagsOrEnvEnablesColor) {
    EXPECT_TRUE(colorEnabled(false, true, false));
}

// --no-color -> off, even on TTY without env.
TEST(ColorEnabled, NoColorFlagDisablesColor) {
    EXPECT_FALSE(colorEnabled(true, true, false));
}

// NO_COLOR=1 -> off, even on TTY without flag.
TEST(ColorEnabled, NoColorEnvDisablesColor) {
    EXPECT_FALSE(colorEnabled(false, true, true));
}

// Piped stdout (not a TTY) -> off, even without flag/env.
TEST(ColorEnabled, NonTtyDisablesColor) {
    EXPECT_FALSE(colorEnabled(false, false, false));
}

// Flag and env together also off (documents precedence, both disable).
TEST(ColorEnabled, FlagAndEnvTogetherDisablesColor) {
    EXPECT_FALSE(colorEnabled(true, true, true));
}

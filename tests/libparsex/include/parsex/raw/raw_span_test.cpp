#include <gtest/gtest.h>

#include <parsex/raw/raw_span.hpp>

TEST(RawSpanTest, IsValidReturnsTrueWhenStartExceedsEnd) {
    const RawSpan span{.startOffset = 10, .endOffset = 5, .lineNumber = 7};

    EXPECT_TRUE(span.isValid());
}

TEST(RawSpanTest, IsValidReturnsFalseWhenStartDoesNotExceedEnd) {
    const RawSpan span{.startOffset = 5, .endOffset = 10, .lineNumber = 7};

    EXPECT_FALSE(span.isValid());
}

TEST(RawSpanTest, DefaultSpanIsNotValid) {
    const RawSpan span{};

    EXPECT_FALSE(span.isValid());
}

TEST(RawSpanTest,SpanEqualityPositive)
{
    const RawSpan span1{.startOffset = 5};
    const RawSpan span2{.startOffset = 5};

    EXPECT_EQ(span1,span2);
}

TEST(RawSpanTest,SpanEqualityNegative)
{
    const RawSpan span1{.startOffset = 5};
    const RawSpan span2{.startOffset = 6};

    EXPECT_NE(span1,span2);
}
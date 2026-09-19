// Model defaults checklist — compare directly against docs/lld.md's Parser data model.
//
// One TEST per domain type. Each test default-constructs the type and asserts
// every field the LLD calls out with a default or a specific required-ness.
// If a future refactor changes a default or flips optional<T> <-> T, a test
// here must fail.
//
// Conventions locked down by this file:
// - Frame.length / Pdu.length default to 0, the "not yet set" sentinel.
// - Signal.factor defaults to 1.0, Signal.offset defaults to 0.0
//   (identity scaling; also asserted in signal_test.cpp — consolidated here).
// - CommonFields.category defaults to std::nullopt (absent), NOT "".
//   nullopt = "no category was given"; "" = "category is the empty string".
// - Signal.valueTable defaults to std::nullopt (absent), NOT an empty vector.
//   Same empty-vs-absent distinction: check .has_value(), not .empty().

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <parsex/model/cluster.hpp>
#include <parsex/model/common_fields.hpp>
#include <parsex/model/ecu_instance.hpp>
#include <parsex/model/frame.hpp>
#include <parsex/model/pdu.hpp>
#include <parsex/model/signal.hpp>
#include <parsex/model/signal_group.hpp>
#include <parsex/raw/raw_span.hpp>

// Compile-time locks on the optional-vs-empty distinction: changing
// CommonFields::category to std::string or Signal::valueTable to a bare
// vector must break the build, not just silently change behaviour.
static_assert(std::is_same_v<decltype(CommonFields::category), std::optional<std::string>>);
static_assert(
    std::is_same_v<decltype(Signal::valueTable), std::optional<std::vector<ValueTableEntry>>>);

TEST(ModelDefaults, CommonFields)
{
    CommonFields common{};

    // shortName is required: no meaningful default, starts empty until set.
    EXPECT_EQ(common.shortName, "");
    // category is optional: absent (nullopt), not "present but empty".
    EXPECT_EQ(common.category, std::nullopt);
    EXPECT_FALSE(common.category.has_value());
    // No source location yet.
    EXPECT_EQ(common.rawSpanRef, (RawSpan{}));
}

TEST(ModelDefaults, Signal)
{
    Signal sig{};

    EXPECT_EQ(sig.common.shortName, "");
    EXPECT_EQ(sig.common.category, std::nullopt);
    EXPECT_EQ(sig.common.rawSpanRef, (RawSpan{}));

    EXPECT_EQ(sig.startBit, 0U);
    EXPECT_EQ(sig.bitLength, 0U);
    EXPECT_EQ(sig.byteOrder, ByteOrder::MostSignificantByteFirst);
    EXPECT_FALSE(sig.isSigned);

    // Identity scaling.
    EXPECT_DOUBLE_EQ(sig.factor, 1.0);
    EXPECT_DOUBLE_EQ(sig.offset, 0.0);

    EXPECT_EQ(sig.min, std::nullopt);
    EXPECT_EQ(sig.max, std::nullopt);
    EXPECT_EQ(sig.unit, std::nullopt);
    EXPECT_EQ(sig.initValue, std::nullopt);

    EXPECT_TRUE(sig.receivers.empty());

    // Absent value table (nullopt), not "present but with zero entries".
    EXPECT_EQ(sig.valueTable, std::nullopt);
    EXPECT_FALSE(sig.valueTable.has_value());
}

TEST(ModelDefaults, SignalGroup)
{
    SignalGroup group{};

    EXPECT_EQ(group.common.shortName, "");
    EXPECT_EQ(group.common.category, std::nullopt);
    EXPECT_EQ(group.common.rawSpanRef, (RawSpan{}));
    EXPECT_TRUE(group.members.empty());
}

TEST(ModelDefaults, Frame)
{
    Frame frame{};

    EXPECT_EQ(frame.common.shortName, "");
    EXPECT_EQ(frame.common.category, std::nullopt);
    EXPECT_EQ(frame.common.rawSpanRef, (RawSpan{}));

    // "Not yet set" sentinel.
    EXPECT_EQ(frame.length, 0U);

    EXPECT_TRUE(frame.transmitters.empty());
    EXPECT_TRUE(frame.pdus.empty());
}

TEST(ModelDefaults, Pdu)
{
    Pdu pdu{};

    EXPECT_EQ(pdu.common.shortName, "");
    EXPECT_EQ(pdu.common.category, std::nullopt);
    EXPECT_EQ(pdu.common.rawSpanRef, (RawSpan{}));

    // "Not yet set" sentinel.
    EXPECT_EQ(pdu.length, 0U);

    EXPECT_TRUE(pdu.signalMappings.empty());
}

TEST(ModelDefaults, Cluster)
{
    Cluster cluster{};

    EXPECT_EQ(cluster.common.shortName, "");
    EXPECT_EQ(cluster.common.category, std::nullopt);
    EXPECT_EQ(cluster.common.rawSpanRef, (RawSpan{}));

    EXPECT_EQ(cluster.baudrate, std::nullopt);
    EXPECT_TRUE(cluster.physicalChannels.empty());
}

TEST(ModelDefaults, EcuInstance)
{
    EcuInstance ecu{};

    EXPECT_EQ(ecu.common.shortName, "");
    EXPECT_EQ(ecu.common.category, std::nullopt);
    EXPECT_EQ(ecu.common.rawSpanRef, (RawSpan{}));

    EXPECT_TRUE(ecu.connectedChannels.empty());
    EXPECT_TRUE(ecu.controllers.empty());
}

TEST(ModelDefaults, MappingAndValueTableEntryStructs)
{
    PduSignalMapping psm{};
    EXPECT_EQ(psm.signalShortNameRef, "");
    EXPECT_EQ(psm.startPosition, 0U);
    EXPECT_EQ(psm.byteOrder, ByteOrder::MostSignificantByteFirst);

    FramePduMapping fpm{};
    EXPECT_EQ(fpm.pduShortNameRef, "");
    EXPECT_EQ(fpm.startPosition, 0U);

    ValueTableEntry entry{};
    EXPECT_EQ(entry.value, std::int64_t{0});
    EXPECT_EQ(entry.label, "");
}

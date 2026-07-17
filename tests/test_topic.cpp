#include <doctest/doctest.h>

#include <string>

#include "picohome/net/topic.h"

using picohome::buildTopic;
using picohome::DeviceType;
using picohome::ParsedTopic;
using picohome::parseTopic;

TEST_CASE("buildTopic formats the 5-segment scheme") {
    char buffer[64];
    REQUIRE(buildTopic(buffer, sizeof buffer, "utility", "kitchen", DeviceType::Light, 29,
                       "set_binary"));
    CHECK(std::string(buffer) == "utility/kitchen/light/29/set_binary");
}

TEST_CASE("buildTopic fails cleanly on a too-small buffer") {
    char buffer[8];
    CHECK_FALSE(buildTopic(buffer, sizeof buffer, "utility", "kitchen", DeviceType::Light, 29,
                           "set_binary"));
}

TEST_CASE("parseTopic round-trips a built topic") {
    ParsedTopic t;
    REQUIRE(parseTopic("utility/kitchen/light/29/set_binary", t));
    CHECK(t.building == "utility");
    CHECK(t.room == "kitchen");
    CHECK(t.type == DeviceType::Light);
    CHECK(t.id == 29);
    CHECK(t.command == "set_binary");
}

TEST_CASE("parseTopic accepts every known device type segment") {
    ParsedTopic t;
    REQUIRE(parseTopic("closet/office/dimmable_light/32/set_range", t));
    CHECK(t.type == DeviceType::DimmableLight);
    REQUIRE(parseTopic("sauna/sauna/temperature/760/measurement", t));
    CHECK(t.type == DeviceType::Temperature);
    REQUIRE(parseTopic("utility/controller/status/0/alive", t));
    CHECK(t.type == DeviceType::Status);
    CHECK(t.id == 0);
}

TEST_CASE("parseTopic rejects malformed topics") {
    ParsedTopic t;
    CHECK_FALSE(parseTopic("too/few/segments", t));
    CHECK_FALSE(parseTopic("a/b/light/29/cmd/extra", t));
    CHECK_FALSE(parseTopic("a/b/light/notanumber/cmd", t));
    CHECK_FALSE(parseTopic("a/b/nonsense_type/1/cmd", t));
    CHECK_FALSE(parseTopic("a//light/1/cmd", t));
    CHECK_FALSE(parseTopic("", t));
}
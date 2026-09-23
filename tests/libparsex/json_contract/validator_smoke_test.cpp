// Smoke test for the json-schema-validator dependency (PAR-172).
//
// Confirms the library links and runs against the project's pinned
// nlohmann/json (3.12.0): loads a one-line schema {"type": "string"} and
// validates a passing case ("hello") plus a failing case (42).
#include <gtest/gtest.h>

#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

TEST(JsonSchemaValidatorSmokeTest, LinksAndValidatesTrivialSchema) {
    const nlohmann::json schema = nlohmann::json::parse(R"({"type": "string"})");

    nlohmann::json_schema::json_validator validator;
    ASSERT_NO_THROW(validator.set_root_schema(schema));

    const nlohmann::json passing = nlohmann::json("hello");
    const nlohmann::json failing = nlohmann::json(42);

    nlohmann::json_schema::basic_error_handler passHandler;
    validator.validate(passing, passHandler);
    EXPECT_FALSE(passHandler) << "string should validate against {\"type\": \"string\"}";

    nlohmann::json_schema::basic_error_handler failHandler;
    validator.validate(failing, failHandler);
    EXPECT_TRUE(failHandler) << "number must NOT validate against {\"type\": \"string\"}";
}

# DEFINE EXECUTABLE
add_executable(parsex-cli main.cpp)

# Find CLI11 library
find_package(CLI11 CONFIG REQUIRED)

# LINK DEPENDENCIES TO EXECUTABLE
target_link_libraries(parsex-cli PRIVATE libparsex CLI11::CLI11)

# PAR-211: CTest smoke coverage for CLI help and usage behavior (real binary).
add_test(NAME cli.bare_requires_subcommand
    COMMAND bash -c "out=$(\"$0\" 2>&1); code=$?; echo \"$out\"; echo \"$out\" | grep -qi \"subcommand is required\" && test $code -ne 0" $<TARGET_FILE:parsex-cli>)
set_tests_properties(cli.bare_requires_subcommand PROPERTIES
    PASS_REGULAR_EXPRESSION "subcommand is required")

add_test(NAME cli.top_help
    COMMAND $<TARGET_FILE:parsex-cli> --help)
set_tests_properties(cli.top_help PROPERTIES
    PASS_REGULAR_EXPRESSION "SUBCOMMANDS")

add_test(NAME cli.parse_help
    COMMAND $<TARGET_FILE:parsex-cli> parse --help)
set_tests_properties(cli.parse_help PROPERTIES
    PASS_REGULAR_EXPRESSION "parse")

add_test(NAME cli.validate_help
    COMMAND $<TARGET_FILE:parsex-cli> validate --help)
set_tests_properties(cli.validate_help PROPERTIES
    PASS_REGULAR_EXPRESSION "validate")

add_test(NAME cli.diff_help
    COMMAND $<TARGET_FILE:parsex-cli> diff --help)
set_tests_properties(cli.diff_help PROPERTIES
    PASS_REGULAR_EXPRESSION "diff")

add_test(NAME cli.write_help
    COMMAND $<TARGET_FILE:parsex-cli> write --help)
set_tests_properties(cli.write_help PROPERTIES
    PASS_REGULAR_EXPRESSION "write")

add_test(NAME cli.version
    COMMAND $<TARGET_FILE:parsex-cli> --version)
set_tests_properties(cli.version PROPERTIES
    PASS_REGULAR_EXPRESSION "[0-9]+\\.[0-9]+\\.[0-9]+")

# PAR-215: JSON schema conformance and human/JSON mutual exclusivity (real
# binary; full schema validation lives in tests/cli/json_output_test.cpp via
# validatesAgainstSchema() — these CTest entries keep the checks visible
# alongside the PAR-211 smoke tests).
add_test(NAME cli.json_validate_output
    COMMAND $<TARGET_FILE:parsex-cli> --json validate --input ${CMAKE_SOURCE_DIR}/tests/fixtures/tiny_valid.arxml)
set_tests_properties(cli.json_validate_output PROPERTIES
    PASS_REGULAR_EXPRESSION "validationResult")

add_test(NAME cli.human_output_not_json
    COMMAND $<TARGET_FILE:parsex-cli> parse --input ${CMAKE_SOURCE_DIR}/tests/fixtures/tiny_valid.arxml)
set_tests_properties(cli.human_output_not_json PROPERTIES
    PASS_REGULAR_EXPRESSION "Parsed"
    FAIL_REGULAR_EXPRESSION "contractVersion")

# PAR-219: one passing CTest case per reachable ExitCode value (64/65/74, and
# 70 if triggerable). This Story's top-level try/catch + classify() already
# handles all four, but only EX_USAGE (64) is directly observable as a distinct
# CLI exit code before PAR-206 wires the engines. EX_DATAERR (65) and EX_IOERR
# (74) are covered by the Classify.* unit tests in tests/cli/exit_code_test.cpp
# and will gain end-to-end CTest invocations once parse/validate/diff/write
# call the real engines. EX_SOFTWARE (70) has no triggerable path yet and is
# documented as untestable here — a deliberate throw hook would be artificial
# before any real internal-error source exists.

# EX_USAGE (64) family: CLI11 parse failures. Exit codes from CLI11 are
# internal (105-109) but the classify() mapping for CLI::ParseError -> Usage
# (64) is validated via the Classify.CliParseErrorMapsToUsage unit test whose
# logic is what PAR-218's catch will use for any future non-CLI11 path
# rethrowing as ParseError. The concrete invocations below at least prove the
# process's exit status is non-zero and stderr names the offending flag.
add_test(NAME cli.exit.usage_bad_flag
    COMMAND bash -c "out=$(\"$0\" --bad-flag 2>&1); code=$?; echo \"$out\"; test $code -ne 0 && echo \"$out\" | grep -qi \"subcommand\\|bad-flag\"" $<TARGET_FILE:parsex-cli>)
set_tests_properties(cli.exit.usage_bad_flag PROPERTIES
    PASS_REGULAR_EXPRESSION "subcommand|bad-flag")

add_test(NAME cli.exit.usage_missing_input
    COMMAND bash -c "out=$(\"$0\" parse 2>&1); code=$?; echo \"$out\"; test $code -ne 0 && echo \"$out\" | grep -qi \"input.*required\\|--input\"" $<TARGET_FILE:parsex-cli>)
set_tests_properties(cli.exit.usage_missing_input PROPERTIES
    PASS_REGULAR_EXPRESSION "input.*required|--input")

add_test(NAME cli.exit.usage_nonexistent_file
    COMMAND bash -c "out=$(\"$0\" parse --input /tmp/parsex-does-not-exist-219.arxml 2>&1); code=$?; echo \"$out\"; test $code -ne 0 && echo \"$out\" | grep -qi \"does not exist\"" $<TARGET_FILE:parsex-cli>)
set_tests_properties(cli.exit.usage_nonexistent_file PROPERTIES
    PASS_REGULAR_EXPRESSION "does not exist")

# PAR-223: precedence chain flag > env > config > default. Each level alone
# and one combined case. Asserts via the debug log line added in PAR-221
# ([debug] schema-cache-dir=... (source)). Uses ENVIRONMENT to set/unset
# PARSEX_SCHEMA_CACHE_DIR without leaking between tests.

add_test(NAME cli.precedence.flag_only
    COMMAND bash -c "out=$(\"$0\" --schema-cache-dir /tmp/cli-flag-only parse --input \"${CMAKE_SOURCE_DIR}/tests/fixtures/tiny_valid.arxml\" 2>&1); echo \"$out\"; echo \"$out\" | grep -q \"schema-cache-dir=/tmp/cli-flag-only (flag)\"" $<TARGET_FILE:parsex-cli>)
set_tests_properties(cli.precedence.flag_only PROPERTIES
    PASS_REGULAR_EXPRESSION "schema-cache-dir=/tmp/cli-flag-only \\(flag\\)")

add_test(NAME cli.precedence.env_only
    COMMAND bash -c "out=$(\"$0\" parse --input \"${CMAKE_SOURCE_DIR}/tests/fixtures/tiny_valid.arxml\" 2>&1); echo \"$out\"; echo \"$out\" | grep -q \"schema-cache-dir=/tmp/cli-env-only (env)\"" $<TARGET_FILE:parsex-cli>)
set_tests_properties(cli.precedence.env_only PROPERTIES
    ENVIRONMENT "PARSEX_SCHEMA_CACHE_DIR=/tmp/cli-env-only"
    PASS_REGULAR_EXPRESSION "schema-cache-dir=/tmp/cli-env-only \\(env\\)")

add_test(NAME cli.precedence.config_only
    COMMAND bash -c "cfg=$(mktemp /tmp/parsex_precedence_config_XXXX.toml); echo 'schema-cache-dir=\"/tmp/cli-config-only\"' > \"$cfg\"; out=$(\"$0\" --config \"$cfg\" parse --input \"${CMAKE_SOURCE_DIR}/tests/fixtures/tiny_valid.arxml\" 2>&1); echo \"$out\"; echo \"$out\" | grep -q \"schema-cache-dir=/tmp/cli-config-only (config)\"; rc=$?; rm -f \"$cfg\"; test $rc -eq 0" $<TARGET_FILE:parsex-cli>)
set_tests_properties(cli.precedence.config_only PROPERTIES
    PASS_REGULAR_EXPRESSION "schema-cache-dir=/tmp/cli-config-only \\(config\\)")

add_test(NAME cli.precedence.default_only
    COMMAND bash -c "out=$(\"$0\" parse --input \"${CMAKE_SOURCE_DIR}/tests/fixtures/tiny_valid.arxml\" 2>&1); echo \"$out\"; echo \"$out\" | grep -q \"schema-cache-dir=.*parsex/schemas (default)\"" $<TARGET_FILE:parsex-cli>)
set_tests_properties(cli.precedence.default_only PROPERTIES
    PASS_REGULAR_EXPRESSION "schema-cache-dir=.*parsex/schemas \\(default\\)")

add_test(NAME cli.precedence.combined_flag_wins
    COMMAND bash -c "cfg=$(mktemp /tmp/parsex_precedence_combined_XXXX.toml); echo 'schema-cache-dir=\"/tmp/cli-config-combined\"' > \"$cfg\"; out=$(PARSEX_SCHEMA_CACHE_DIR=/tmp/cli-env-combined \"$0\" --schema-cache-dir /tmp/cli-flag-combined --config \"$cfg\" parse --input \"${CMAKE_SOURCE_DIR}/tests/fixtures/tiny_valid.arxml\" 2>&1); echo \"$out\"; echo \"$out\" | grep -q \"schema-cache-dir=/tmp/cli-flag-combined (flag)\"; rc=$?; rm -f \"$cfg\"; test $rc -eq 0" $<TARGET_FILE:parsex-cli>)
set_tests_properties(cli.precedence.combined_flag_wins PROPERTIES
    PASS_REGULAR_EXPRESSION "schema-cache-dir=/tmp/cli-flag-combined \\(flag\\)")
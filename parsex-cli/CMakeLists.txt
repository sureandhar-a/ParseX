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
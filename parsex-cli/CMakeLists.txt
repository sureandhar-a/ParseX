# DEFINE EXECUTABLE
add_executable(parsex-cli main.cpp)

# Find CLI11 library
find_package(CLI11 CONFIG REQUIRED)

# LINK DEPENDENCIES TO EXECUTABLE
target_link_libraries(parsex-cli PRIVATE libparsex CLI11::CLI11)
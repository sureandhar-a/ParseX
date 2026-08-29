#include <iostream>
#include "CLI/CLI.hpp"

using namespace std;

int main(int argc, char const *argv[])
{
    CLI::App app{"ParseX CLI", "ParseX"};
    CLI11_PARSE(app, argc, argv);
    return 0;
}

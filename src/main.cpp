#include <iostream>
#include <string>

#include "common/version.hpp"

int main(int argc, char* argv[]) {
    if (argc > 1) {
        const std::string arg = argv[1];
        if (arg == "--version") {
            std::cout << bms::kProjectName << ' ' << bms::kVersion << '\n';
            return 0;
        }
        if (arg == "--help") {
            std::cout << "Usage: bms_server [--version] [--help]\n";
            return 0;
        }
        std::cerr << "Unknown option: " << arg << "\nRun bms_server --help for usage.\n";
        return 2;
    }

    std::cout << bms::kProjectName << ' ' << bms::kVersion << '\n'
              << "The HTTP server is not implemented yet (S-04).\n";
    return 0;
}

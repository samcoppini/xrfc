#include "CLI/CLI.hpp"

#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    CLI::App app("Compiles XRF files.");

    std::string filename;

    app.add_option("file", filename, "The XRF file to compile.")
        ->required();

    CLI11_PARSE(app, argc, argv);

    std::ifstream file{filename};

    if (!file.is_open()) {
        std::cerr << "Unable to open " << filename << '\n';
        return 1;
    }
}

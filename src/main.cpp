#include "parser.hpp"

#include "CLI/CLI.hpp"

#include <fstream>
#include <iostream>

void printParserErrors(const xrf::ParserErrorList &errors) {
    int numErrors = 0;

    for (auto &error: errors) {
        if (numErrors++ == 100) {
            std::cerr << "Too many errors, quitting." << "\n";
            return;
        }

        std::cerr << "Error on line " << error.line << ", column " << error.col
                  << ": " << error.msg << '\n';
    }
}

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

    xrf::FileReader fileReader(file);
    auto result = xrf::parseXrf(fileReader);

    if (auto *errors = std::get_if<xrf::ParserErrorList>(&result); errors) {
        printParserErrors(*errors);
        return 2;
    }
}

#include "codegen.hpp"
#include "optimization.hpp"
#include "parser.hpp"

#include "CLI/CLI.hpp"
#include "llvm/Support/raw_ostream.h"

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
    std::string outFilename;
    int optimizationLevel = 0;

    app.add_option("file", filename, "The XRF file to compile.")
        ->required();
    app.add_option("-o,--output", outFilename, "The file to write the compiled source to.");
    app.add_option("-O", optimizationLevel,
        "The level of optimization for XRF code.\n0 = none\n1 = chunk-level optimizations\n2 = program-level optimizations");

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

    llvm::LLVMContext context;
    auto chunks = std::get<std::vector<xrf::Chunk>>(result);

    if (optimizationLevel > 0) {
        chunks = xrf::optimizeChunks(chunks);

        if (optimizationLevel > 1) {
            chunks = xrf::optimizeProgram(chunks);
        }
    }

    auto module = generateCode(context, chunks);

    if (outFilename.empty()) {
        outFilename = "out.ll";
    }

    std::error_code err;
    llvm::raw_fd_ostream outFile(outFilename, err);

    if (err) {
        std::cerr << "Unable to write to " << outFilename << '\n';
        return 3;
    }

    module->print(outFile, nullptr);
    return 0;
}

#pragma once

#include "file-reader.hpp"
#include "xrf-chunk.hpp"

#include <string>
#include <variant>
#include <vector>

namespace xrf {

struct ErrorMsg {
    // The error message to display
    std::string msg;

    // The line and column the error occurred on
    int line, col;

    ErrorMsg(const std::string &msg, int line, int col):
        msg(msg), line(line), col(col)
    {}
};

using ParserErrorList = std::vector<ErrorMsg>;

// Parses a file, either returning a list of errors if there are any errors,
// or a list of the chunks if the file is valid XRF source
std::variant<ParserErrorList, std::vector<Chunk>> parseXrf(FileReader &);

} // namespace xrf

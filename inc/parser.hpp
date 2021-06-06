#pragma once

#include "file-reader.hpp"
#include "xrf-chunk.hpp"

#include <string>
#include <variant>
#include <vector>

namespace xrf {

/**
 * A simple struct that contains information on an error message encountered
 * while attempting to parse some XRF code.
 */
struct ErrorMsg {
    /**
     * The error message to display
     */
    std::string msg;

    /**
     * The line and column the error occurred on
     */
    int line, col;

    /**
     * Construct a new ErrorMsg object.
     *
     * @param msg
     *     A string explanation of the error
     * @param line
     *     The line that the error occurred on
     * @param col
     *     The column that the error occurred on
     */
    ErrorMsg(const std::string &msg, int line, int col):
        msg(msg), line(line), col(col)
    {}
};

using ParserErrorList = std::vector<ErrorMsg>;

/**
 * @brief Parses a file, either returning a list of errors if there are any errors,
 * or a list of the chunks if the file is valid XRF source.
 *
 * @param reader
 *     The FileReader that has the XRF source to parse.
 *
 * @return
 *     Either a list of parsing errors, or a list of parsed chunks
 */
std::variant<ParserErrorList, std::vector<Chunk>> parseXrf(FileReader &reader);

} // namespace xrf

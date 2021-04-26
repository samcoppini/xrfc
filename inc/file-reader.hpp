#pragma once

#include <iosfwd>
#include <optional>

namespace xrf {

// A simple wrapper around an istream which keeps track of the line and column
// of the last-read character from the file
class FileReader {
public:
    FileReader(std::istream &file);

    std::optional<char> read();

    bool ended() const;

    int curColumn() const;

    int curLine() const;

private:
    std::istream &_file;
    int _line, _col;
};

} // namespace xrf

#include "file-reader.hpp"

#include <istream>

namespace xrf {

FileReader::FileReader(std::istream &file)
    : _file(file)
    , _line(1)
    , _col(0)
{}

std::optional<char> FileReader::read() {
    char c;

    if (!_file.get(c)) {
        return std::nullopt;
    }

    if (c == '\n') {
        _line++;
        _col = 0;
    }
    else {
        _col++;
    }

    return c;
}

bool FileReader::ended() const {
    return _file.eof();
}

int FileReader::curColumn() const {
    return _col;
}

int FileReader::curLine() const {
    return _line;
}

} // namespace xrf

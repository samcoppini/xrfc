#pragma once

#include <iosfwd>
#include <optional>

namespace xrf {

/**
 * A simple wrapper around an istream which keeps track of the line and column
 * of the last-read character from the file.
 */
class FileReader {
public:
    /**
     * Construct a new FileReader object.
     *
     * @param file
     *     The file that this reader wraps around
     */
    FileReader(std::istream &file);

    /**
     * Gets the next character from the file, or std::nullopt if the file has
     * ended.
     *
     * @return std::optional<char>
     *     The next character in the file, or nullopt if the file has ended
     */
    std::optional<char> read();

    /**
     * Returns whether the file has ended.
     *
     * @return
     *     Whether the file has ended
     */
    bool ended() const;

    /**
     * Returns the column of the last-read character from the file.
     *
     * @return
     *     The column of the last-read character
     */
    int curColumn() const;

    /**
     * Returns the line of the last-read character from the file.
     *
     * @return int
     *     The line of the last-read character
     */
    int curLine() const;

private:
    /**
     * The file that the file reader is wrapped around
     */
    std::istream &_file;

    /**
     * The line of the last-read character
     */
    int _line;

    /**
     * The column of the last-read character
     */
    int _col;
};

} // namespace xrf

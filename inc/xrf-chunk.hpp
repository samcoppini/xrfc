#pragma once

#include <optional>
#include <vector>

namespace xrf {

// The number of commands in each chunk
constexpr int COMMANDS_PER_CHUNK = 5;

// The different commands that XRF has
enum CommandType {
    Input,         // 0
    Output,        // 1
    Pop,           // 2
    Dup,           // 3
    Swap,          // 4
    Inc,           // 5
    Dec,           // 6
    Add,           // 7
    IgnoreFirst,   // 8
    Bottom,        // 9
    Jump,          // A
    Exit,          // B
    IgnoreVisited, // C
    Randomize,     // D
    Sub,           // E
    Nop,           // F
};

// A struct representing a "chunk" of commands in XRF
struct Chunk {
    // The commands in the chunk
    std::vector<CommandType> commands;

    // The line/column of the first command in the chunk
    int line, col;

    // The next chunk to jump to, if known
    std::optional<unsigned> nextChunk;
};

} // namespace xrf

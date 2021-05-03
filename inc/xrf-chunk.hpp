#pragma once

#include "xrf-command.hpp"

#include <optional>
#include <vector>

namespace xrf {

// The number of commands in each chunk
constexpr int COMMANDS_PER_CHUNK = 5;

// A struct representing a "chunk" of commands in XRF
struct Chunk {
    // The commands in the chunk
    std::vector<Command> commands;

    // The line/column of the first command in the chunk
    int line, col;

    // The next chunk to jump to, if known
    std::optional<unsigned> nextChunk;
};

} // namespace xrf

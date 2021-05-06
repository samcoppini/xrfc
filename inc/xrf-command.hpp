#pragma once

namespace xrf {

// The different commands that XRF has
enum CommandType {
    // The commands built into XRF
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

    // Commands that get generated from optimization
    PushValueToBottom,
    SetTop,
};

// A struct representing a single stack-based action in XRF
struct Command {
    Command(CommandType type): type(type) {}

    // The type of command this is
    CommandType type;

    unsigned val = 0;
};

} // namespace xrf

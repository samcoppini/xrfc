#pragma once

namespace xrf {

// The different commands that XRF has
enum class CommandType {
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

    // All of the following are commands that don't map directly to specific
    // XRF commands, and are generated as a result of optimization

    // Adds a value to the secondmost value on the stack
    AddToSecond,

    // Multiplies the second value on the stack by a given value
    MultiplySecond,

    // Removes the second value from the stack
    PopSecondValue,

    // Inserts a value below the top of the stack
    PushSecondValue,

    // Pushes a known value to the bottom of the stack, without having to push
    // it to the top of the stack first
    PushValueToBottom,

    // Sets the second value of the stack
    SetSecondValue,

    // Sets the top value of the stack to a known value
    SetTop,
};

// A struct representing a single stack-based action in XRF
struct Command {
    Command(CommandType type): type(type) {}

    Command(CommandType type, int val): type(type), val(val) {}

    // The type of command this is
    CommandType type;

    // For optimized commands, an additional parameter for the command
    int val = 0;
};

} // namespace xrf

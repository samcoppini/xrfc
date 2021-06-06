#pragma once

#include "xrf-command.hpp"
#include "stack-value.hpp"

#include <vector>

namespace xrf {

/**
 * The StackSimulator is used to during optimization to simulate the result of
 * running an XRF code chunk. This is used for two purposes: the first is to
 * see if after running a chunk, if the top value is known, which allows us to
 * jump directly to that code chunk. The other purpose is to see if we can
 * generate a series of optimized instructions to produce the same stack state.
 */
class StackSimulator {
    public:
        /**
         * Construct a new StackSimulator
         *
         * @param index
         *     The chunk index of the chunk we'll be simulating. An invariant
         *     of all XRF code is that at the start of a chunk, the top value
         *     of the stack is the chunk index, so effectively this just creates
         *     the stack simulator initialized with the index on the top.
         */
        StackSimulator(unsigned index);

        /**
         * Simulates adding the top two values of the stack.
         */
        void add();

        /**
         * Simulates sending the top value of the stack to the bottom.
         */
        void bottom();

        /**
         * Simulates decrementing the top value of the stack.
         */
        void dec();

        /**
         * Simulates duplicating the top value of the stack.
         */
        void dup();

        /**
         * Simulates incrementing the top value of the stack.
         */
        void inc();

        /**
         * Simulates a pushing a byte of input onto the stack.
         */
        void input();

        /**
         * Simulates outputting the top value of the stack.
         */
        void output();

        /**
         * Simulates popping the top value of the stack.
         */
        void pop();

        /**
         * Simulates subtracting the top two values of the stack.
         */
        void sub();

        /**
         * Simulates swapping the top two values of the stack.
         */
        void swap();

        /**
         * Returns the top value of the stack, if it is known value, otherwise
         * return std::nullopt.
         *
         * @return
         *     The stack top, if known, otherwise std::nullopt
         */
        std::optional<unsigned> getStackTop();

        /**
         * Tries to create an optimized series of commands that will reproduce
         * the current state of the stack.
         *
         * @return
         *     A series of optimized commands, or std::nullopt if we were
         *     unable optimize the commands.
         */
        std::optional<std::vector<Command>> getCommands() const;

    private:
        /**
         * Returns whether all of the values in the given vector have a
         * statically-known value.
         *
         * @param values
         *     The list of values to check
         *
         * @return
         *     Whether all of the values have known values
         */
        static bool allKnownValues(const std::vector<StackValue> &values);

        /**
         * Returns whether we are able to optimize the current state of the
         * stack into a series of optimized commands.
         *
         * @return
         *     Whether we're able to optimize the stack's state
         */
        bool canOptimize() const;

        /**
         * Simulates pushing a stack value to the stack.
         *
         * @param value
         *     The value to push to the stack
         */
        void doPush(StackValue value);

        /**
         * Simulates popping the stack.
         *
         * @return StackValue
         *     The value that was popped from the stack
         */
        StackValue doPop();

        /**
         * The initial top value of the stack.
         */
        unsigned _origIndex;

        /**
         * How deep into the stack we've popped. This does not mean how many
         * times we popped the stack, but rather the deepest index into the
         * stack we've popped.
         */
        unsigned _maxPopped;

        /**
         * Whether we've simulated an input or output command
         */
        bool _hadIO;

        /**
         * A list of values we've sent to the bottom
         */
        std::vector<StackValue> _bottom;

        /**
         * The list of values that we know to be on the top of the stack
         */
        std::vector<StackValue> _values;
};

} // namespace xrf

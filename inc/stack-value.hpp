#pragma once

#include <optional>

namespace xrf {

/**
 * Represents a value on the stack, for use with the StackSimulator. This keeps
 * track of what information we know about the value, which may include its
 * value, its original index on the stack, and what has been added to it. The
 * operations on StackValue will preserve information if it's known, and produce
 * unknown values if either of the operands is unknown.
 */
class StackValue {
    public:
        /**
         * Construct a new StackValue object with a known value from a knonw
         * index on the stack.
         *
         * @param index
         *     The index the stack value originated from.
         * @param val
         *     The value of the StackValue
         */
        StackValue(unsigned index, unsigned val);

        /**
         * Creates a StackValue which originates from a known index on the
         * stack.
         *
         * @param index
         *     The index that the value originates from
         *
         * @return
         *     A StackValue with a known index
         */
        static StackValue fromIndex(unsigned index);

        /**
         * Creates a StackValue with a known value.
         *
         * @param value
         *     The value of the StackValue
         *
         * @return
         *     A StackValue with a known value
         */
        static StackValue fromValue(unsigned value);

        /**
         * Constructs a StackValue about which nothing is known.
         */
        StackValue() = default;

        /**
         * Simulates adding a StackValue to this one.
         *
         * @param value
         *     The value to add to this value
         */
        void add(const StackValue &value);

        /**
         * Simulates decrementing this value. Note that this is different from
         * calling sub() with a known value of 1, since that can never
         * underflow.
         */
        void dec();

        /**
         * Simulates a subtraction operation on this value. This will subtract
         * the smaller value from the larger one, and sets this value to the
         * difference.
         *
         * @param value
         *     The value to subtract
         */
        void sub(const StackValue &value);

        /**
         * Returns whether this stack value has a statically-known value.
         *
         * @return
         *     Whether this value is known at compile-time
         */
        bool hasKnownValue() const;

        /**
         * Returns the statically-known value of this StackValue. Should not be
         * called if !hasKnownValue().
         *
         * @return
         *     The known value of this StackValue
         */
        unsigned getKnownValue() const;

        /**
         * Returns whether the stack value originates from a known index on
         * the stack.
         *
         * @return
         *     Whether this value is from a known index
         */
        bool hasKnownIndex() const;

        /**
         * Returns the originating index of this StackValue. Should not be
         * called if !hasKnownIndex().
         *
         * @return
         *     The original index of this StackValue
         */
        unsigned getIndex() const;

        /**
         * Returns the value of this StackValue if it's known, or std::nullopt
         * if it's not known.
         *
         * @return
         *     The value of this StackValue if known, std::nullopt otherwise
         */
        std::optional<unsigned> getValue() const;

        /**
         * Returns the amount that this stack value has been changed from its
         * original value. Only valid if hasKnownIndex() && !hasKnownValue().
         *
         * @return
         *     The amount the stack value has been changed
         */
        int getChange() const;

        /**
         * Returns the amount that this stack value has been multiplied by.
         * Only valid if hasKnownIndex() && !hasKnownValue().
         *
         * @return
         *     The amount that the value has been multiplied by
         */
        unsigned getMultiple() const;

    private:
        /**
         * The index that the StackValue originated from, if known.
         */
        std::optional<unsigned> _index;

        /**
         * The value of the StackValue, if known
         */
        std::optional<unsigned> _value;

        /**
         * The amount that the StackValue is changed by
         */
        int _knownChange = 0;

        /**
         * The amount that the StackValue is multiplied by
         */
        unsigned _knownMultiple = 1;
};

} // namespace xrf

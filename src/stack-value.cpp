#include "stack-value.hpp"

namespace xrf {

StackValue::StackValue(unsigned index, unsigned val):
    _index(index),
    _value(val)
{}

StackValue StackValue::fromIndex(unsigned index) {
    StackValue value;
    value._index = index;
    return value;
}

StackValue StackValue::fromValue(unsigned value) {
    StackValue stackValue;
    stackValue._value = value;
    return stackValue;
}

void StackValue::add(const StackValue &value) {
    if (hasKnownValue()) {
        if (value.hasKnownValue()) {
            *_value += value.getKnownValue();
            return;
        }
    }
    else if (hasKnownIndex()) {
        if (value.hasKnownIndex() && value.getIndex() == getIndex()) {
            _knownMultiple += value._knownMultiple;
            return;
        }
        else if (value.hasKnownValue()) {
            _knownChange += value.getKnownValue();
            return;
        }
    }
    _index = std::nullopt;
    _value = std::nullopt;
}

void StackValue::dec() {
    if (hasKnownValue() && getKnownValue() > 0) {
        _value = *_value - 1;
        return;
    }
    _value = std::nullopt;
    _knownChange--;
}

void StackValue::sub(const StackValue &value) {
    if (hasKnownValue() && value.hasKnownValue()) {
        auto val1 = getKnownValue();
        auto val2 = getKnownValue();

        if (val1 > val2) {
            _value = val1 - val2;
        }
        else {
            _value = val2 - val1;
        }
        return;
    }
    _index = std::nullopt;
    _value = std::nullopt;
}

bool StackValue::hasKnownValue() const {
    return _value.has_value();
}

unsigned StackValue::getKnownValue() const {
    return *_value;
}

bool StackValue::hasKnownIndex() const {
    return _index.has_value();
}

unsigned StackValue::getIndex() const {
    return *_index;
}

std::optional<unsigned> StackValue::getValue() const {
    return _value;
}

int StackValue::getChange() const {
    return _knownChange;
}

unsigned StackValue::getMultiple() const {
    return _knownMultiple;
}

} // namespace xrf

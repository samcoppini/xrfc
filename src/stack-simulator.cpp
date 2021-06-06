#include "stack-simulator.hpp"

#include <algorithm>

namespace xrf {

StackSimulator::StackSimulator(unsigned index):
    _origIndex(index),
    _maxPopped(0),
    _hadIO(false),
    _values({{0, index}})
{}

void StackSimulator::add() {
    auto val1 = doPop();
    auto val2 = doPop();

    val1.add(val2);
    doPush(val1);
}

void StackSimulator::bottom() {
    _bottom.push_back(doPop());
}

void StackSimulator::dec() {
    auto val = doPop();
    val.dec();
    doPush(val);
}

void StackSimulator::dup() {
    auto val = doPop();
    doPush(val);
    doPush(val);
}

void StackSimulator::inc() {
    auto val = doPop();
    val.add(StackValue::fromValue(1));
    doPush(val);
}

void StackSimulator::input() {
    doPush({});
    _hadIO = true;
}

void StackSimulator::output() {
    doPop();
    _hadIO = true;
}

void StackSimulator::pop() {
    doPop();
}

void StackSimulator::sub() {
    auto val1 = doPop();
    auto val2 = doPop();
    val1.sub(val2);
    doPush(val1);
}

void StackSimulator::swap() {
    auto val1 = doPop();
    auto val2 = doPop();
    doPush(val1);
    doPush(val2);
}

std::optional<unsigned> StackSimulator::getStackTop() {
    auto val = doPop();
    doPush(val);
    return val.getValue();
}

std::optional<std::vector<Command>> StackSimulator::getCommands() const {
    if (!canOptimize()) {
        return std::nullopt;
    }

    std::vector<Command> commands;
    for (const auto &bottomVal: _bottom) {
        commands.emplace_back(CommandType::PushValueToBottom, bottomVal.getKnownValue());
    }

    if (_origIndex != _values.back().getKnownValue()) {
        commands.emplace_back(CommandType::SetTop);
        commands.back().val = _values.back().getKnownValue();
    }

    if (_values.size() > 1) {
        auto secondValue = _values[0];
        if (secondValue.hasKnownValue()) {
            if (_maxPopped == 0) {
                commands.emplace_back(CommandType::PushSecondValue, secondValue.getKnownValue());
            }
            else {
                commands.emplace_back(CommandType::SetSecondValue, secondValue.getKnownValue());
            }
        }
        else if (secondValue.getMultiple() > 1) {
            commands.emplace_back(CommandType::MultiplySecond, secondValue.getMultiple());
        }
        else if (secondValue.getChange() != 0) {
            commands.emplace_back(CommandType::AddToSecond, secondValue.getChange());
        }
    }
    else if (_maxPopped == 1) {
        commands.emplace_back(CommandType::PopSecondValue);
    }

    return commands;
}

bool StackSimulator::allKnownValues(const std::vector<StackValue> &values) {
    return std::all_of(values.begin(), values.end(), [] (StackValue val) {
        return val.hasKnownValue();
    });
}

bool StackSimulator::canOptimize() const {
    return !_hadIO &&
            _maxPopped < 2 &&
            allKnownValues(_bottom) &&
            _values.size() <= 2 &&
            _values.size() >= 1 &&
            _values.back().hasKnownValue() &&
            (_values.size() == 1 || _values[0].hasKnownValue()
            || (_values[0].hasKnownIndex() && _values[0].getIndex() == 1));
}

void StackSimulator::doPush(StackValue value) {
    _values.emplace_back(value);
}

StackValue StackSimulator::doPop() {
    if (!_values.empty()) {
        auto value = _values.back();
        _values.pop_back();
        return value;
    }
    _maxPopped++;
    return StackValue::fromIndex(_maxPopped);
}

} // namespace xrf

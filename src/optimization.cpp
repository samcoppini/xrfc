#include "optimization.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <unordered_set>

namespace xrf {

namespace {

class StackValue {
    public:
        StackValue(unsigned index, unsigned val):
            _index(index),
            _value(val)
        {}

        static StackValue fromIndex(unsigned index) {
            StackValue value;
            value._index = index;
            return value;
        }

        static StackValue fromValue(unsigned value) {
            StackValue stackValue;
            stackValue._value = value;
            return stackValue;
        }

        StackValue() = default;

        void add(const StackValue &value) {
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

        void dec() {
            if (hasKnownValue() && getKnownValue() > 0) {
                _value = *_value - 1;
                return;
            }
            _value = std::nullopt;
            _knownChange--;
        }

        void sub(const StackValue &value) {
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

        bool hasKnownValue() const {
            return _value.has_value();
        }

        unsigned getKnownValue() const {
            return *_value;
        }

        bool hasKnownIndex() const {
            return _index.has_value();
        }

        unsigned getIndex() const {
            return *_index;
        }

        std::optional<unsigned> getValue() const {
            return _value;
        }

        int getChange() const {
            return _knownChange;
        }

        unsigned getMultiple() const {
            return _knownMultiple;
        }

    private:
        std::optional<unsigned> _index;
        std::optional<unsigned> _value;
        int _knownChange = 0;
        unsigned _knownMultiple = 1;
};

class StackSimulator {
    public:
        StackSimulator(unsigned index):
            _origIndex(index),
            _maxPopped(0),
            _hadIO(false),
            _values({{0, index}})
        {}

        void add() {
            auto val1 = doPop();
            auto val2 = doPop();

            val1.add(val2);
            doPush(val1);
        }

        void bottom() {
            _bottom.push_back(doPop());
        }

        void dec() {
            auto val = doPop();
            val.dec();
            doPush(val);
        }

        void dup() {
            auto val = doPop();
            doPush(val);
            doPush(val);
        }

        void inc() {
            auto val = doPop();
            val.add(StackValue::fromValue(1));
            doPush(val);
        }

        void input() {
            doPush({});
            _hadIO = true;
        }

        void output() {
            doPop();
            _hadIO = true;
        }

        void pop() {
            doPop();
        }

        void sub() {
            auto val1 = doPop();
            auto val2 = doPop();
            val1.sub(val2);
            doPush(val1);
        }

        void swap() {
            auto val1 = doPop();
            auto val2 = doPop();
            doPush(val1);
            doPush(val2);
        }

        std::optional<unsigned> getStackTop() {
            auto val = doPop();
            doPush(val);
            return val.getValue();
        }

        std::optional<std::vector<Command>> getCommands() const {
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

    private:
        static bool allKnownValues(const std::vector<StackValue> &values) {
            return std::all_of(values.begin(), values.end(), [] (StackValue val) {
                return val.hasKnownValue();
            });
        }

        bool canOptimize() const {
            return !_hadIO &&
                   _maxPopped < 2 &&
                   allKnownValues(_bottom) &&
                   _values.size() <= 2 &&
                   _values.size() >= 1 &&
                   _values.back().hasKnownValue() &&
                   (_values.size() == 1 || _values[0].hasKnownValue()
                    || (_values[0].hasKnownIndex() && _values[0].getIndex() == 1));
        }

        void doPush(StackValue value) {
            _values.emplace_back(value);
        }

        StackValue doPop() {
            if (!_values.empty()) {
                auto value = _values.back();
                _values.pop_back();
                return value;
            }
            _maxPopped++;
            return StackValue::fromIndex(_maxPopped);
        }

        unsigned _origIndex;
        unsigned _maxPopped;
        bool _hadIO;
        std::vector<StackValue> _bottom;
        std::vector<StackValue> _values;
};

Chunk optimizeChunk(const Chunk &chunk, unsigned index) {
    StackSimulator stack(index);
    Chunk optimized = chunk;

    bool canOptimize = true;

    for (const auto &cmd: chunk.commands) {
        bool breakOut = false;

        switch (cmd.type) {
            case CommandType::Add:
                stack.add();
                break;

            case CommandType::Bottom:
                stack.bottom();
                break;

            case CommandType::Output:
                stack.output();
                break;

            case CommandType::Pop:
                stack.pop();
                break;

            case CommandType::Dec:
                stack.dec();
                break;

            case CommandType::Dup:
                stack.dup();
                break;

            case CommandType::Inc:
                stack.inc();
                break;

            case CommandType::Input:
                stack.input();
                break;

            case CommandType::Jump:
                breakOut = true;
                break;

            case CommandType::Nop:
                break;

            case CommandType::Sub:
                stack.sub();
                break;

            case CommandType::Swap:
                stack.swap();
                break;

            case CommandType::Exit:
            case CommandType::Randomize:
            case CommandType::IgnoreFirst:
            case CommandType::IgnoreVisited:
                canOptimize = false;
                break;

            case CommandType::AddToSecond:
            case CommandType::MultiplySecond:
            case CommandType::PopSecondValue:
            case CommandType::PushSecondValue:
            case CommandType::PushValueToBottom:
            case CommandType::SetSecondValue:
            case CommandType::SetTop:
                assert(false);
                break;
        }

        if (breakOut || !canOptimize) {
            break;
        }
    }

    if (canOptimize) {
        if (auto stackTop = stack.getStackTop(); stackTop) {
            optimized.nextChunk = *stackTop;
        }

        if (auto optimizedCommands = stack.getCommands(); optimizedCommands) {
            optimized.commands = *optimizedCommands;
        }
    }

    return optimized;
}

template <size_t N>
bool chunkOnlyHas(const Chunk &chunk, const std::array<CommandType, N> &commands) {
    return std::all_of(chunk.commands.begin(), chunk.commands.end(), [&] (Command cmd) {
        return std::find(commands.begin(), commands.end(), cmd.type) != commands.end();
    });
}

void condenseStackTops(std::vector<Command> &commands) {
    bool foundStackTop = false;

    for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
        if (it->type == CommandType::SetTop) {
            if (foundStackTop) {
                commands.erase(std::next(it).base());
            }
            foundStackTop = true;
        }
    }
}

void handleSecondVal(std::vector<Command> &commands) {

}

Chunk optimizeChunkInProgram(const std::vector<Chunk> &chunks, size_t index) {
    auto &originalChunk = chunks[index];
    Chunk optimizedChunk;

    auto toOptimize = chunks[index];

    std::unordered_set<size_t> visited;

    while (chunkOnlyHas(toOptimize, std::array{CommandType::AddToSecond,
                                               CommandType::MultiplySecond,
                                               CommandType::PushSecondValue,
                                               CommandType::SetSecondValue,
                                               CommandType::SetTop}))
    {
        if (visited.count(index)) {
            // We're in a infinite loop, don't bother optimizing it
            return originalChunk;
        }
        visited.insert(index);

        optimizedChunk.commands.insert(optimizedChunk.commands.end(),
                                       toOptimize.commands.begin(),
                                       toOptimize.commands.end());

        optimizedChunk.nextChunk = toOptimize.nextChunk;
        index = *optimizedChunk.nextChunk;
        toOptimize = chunks[index];
    }

    if (optimizedChunk.commands.empty()) {
        return originalChunk;
    }

    condenseStackTops(optimizedChunk.commands);

    return optimizedChunk;
}

} // anonymous namespace

std::vector<Chunk> optimizeChunks(const std::vector<Chunk> &chunks) {
    std::vector<Chunk> optimizedChunks;

    for (size_t i = 0; i < chunks.size(); i++) {
        optimizedChunks.emplace_back(optimizeChunk(chunks[i], i));
    }

    return optimizedChunks;
}

std::vector<Chunk> optimizeProgram(const std::vector<Chunk> &chunks) {
    std::vector<Chunk> optimizedProgram;

    optimizedProgram.reserve(chunks.size());

    for (size_t i = 0; i < chunks.size(); i++) {
        optimizedProgram.emplace_back(optimizeChunkInProgram(chunks, i));
    }

    return optimizedProgram;
}

} // namespace xrf

#include "optimization.hpp"

namespace xrf {

namespace {

class StackValue {
    public:
        StackValue(unsigned val):
            _value(val)
        {}

        StackValue() = default;

        void add(const StackValue &value) {
            if (hasKnownValue() && value.hasKnownValue()) {
                *_value += value.getKnownValue();
                return;
            }
            _value = std::nullopt;
        }

        void dec() {
            if (hasKnownValue() && getKnownValue() > 0) {
                _value = *_value - 1;
                return;
            }
            _value = std::nullopt;
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
            _value = std::nullopt;
        }

        bool hasKnownValue() const {
            return _value.has_value();
        }

        unsigned getKnownValue() const {
            return *_value;
        }

        std::optional<unsigned> getValue() const {
            return _value;
        }

    private:
        std::optional<unsigned> _value;
};

class StackSimulator {
    public:
        StackSimulator(unsigned index):
            _values({index})
        {}

        void add() {
            auto val1 = doPop();
            auto val2 = doPop();

            val1.add(val2);
            doPush(val1);
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
            val.add(1);
            doPush(val);
        }

        void input() {
            doPush({});
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

    private:
        void doPush(StackValue value) {
            _values.emplace_back(value);
        }

        StackValue doPop() {
            if (!_values.empty()) {
                auto value = _values.back();
                _values.pop_back();
                return value;
            }
            return {};
        }

        std::vector<StackValue> _values;
};

Chunk optimizeChunk(const Chunk &chunk, unsigned index) {
    StackSimulator stack(index);
    Chunk optimized = chunk;

    bool canOptimize = true;

    for (const auto &cmd: chunk.commands) {
        bool breakOut = false;

        switch (cmd) {
            case CommandType::Add:
                stack.add();
                break;

            case CommandType::Bottom:
            case CommandType::Output:
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
        }

        if (breakOut || !canOptimize) {
            break;
        }
    }

    if (canOptimize) {
        if (auto stackTop = stack.getStackTop(); stackTop) {
            optimized.nextChunk = *stackTop;
        }
    }

    return optimized;
}

} // anonymous namespace

std::vector<Chunk> optimizeChunks(const std::vector<Chunk> &chunks) {
    std::vector<Chunk> optimizedChunks;

    for (size_t i = 0; i < chunks.size(); i++) {
        optimizedChunks.emplace_back(optimizeChunk(chunks[i], i));
    }

    return optimizedChunks;
}

} // namespace xrf
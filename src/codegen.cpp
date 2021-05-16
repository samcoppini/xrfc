#include "codegen.hpp"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"

namespace xrf {

namespace {

constexpr int STACK_SIZE = 65536;
constexpr int STACK_MASK = 65535;

struct XrfContext {
    llvm::Module *module;

    llvm::GlobalVariable *stack;

    llvm::FunctionCallee getcharFunc;

    llvm::FunctionCallee putcharFunc;

    llvm::Function *mainFunc;

    llvm::BasicBlock *startBlock;

    llvm::AllocaInst *stackTop;

    llvm::AllocaInst *stackBottom;

    llvm::AllocaInst *topValue;
};

void generateCodeForChunk(llvm::LLVMContext &context, XrfContext &xrfContext, const Chunk &chunk, size_t index,
                          llvm::BasicBlock *chunkBlock, std::vector<llvm::BasicBlock*> chunks, llvm::BasicBlock *stackJump,
                          bool setVisited = false);

XrfContext getGenerationContext(llvm::Module &module, llvm::LLVMContext &context) {
    XrfContext xrfContext;

    xrfContext.module = &module;

    auto stackType = llvm::ArrayType::get(llvm::IntegerType::getInt32Ty(context), STACK_SIZE);

    module.getOrInsertGlobal("stack", stackType);
    xrfContext.stack = module.getGlobalVariable("stack");
    xrfContext.stack->setLinkage(llvm::GlobalValue::PrivateLinkage);
    xrfContext.stack->setInitializer(llvm::UndefValue::get(stackType));

    xrfContext.getcharFunc = module.getOrInsertFunction(
        "getchar", llvm::IntegerType::getInt32Ty(context));

    xrfContext.putcharFunc = module.getOrInsertFunction(
        "putchar", llvm::IntegerType::getInt32Ty(context), llvm::IntegerType::getInt32Ty(context));

    xrfContext.mainFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(context), false),
        llvm::Function::ExternalLinkage, "main", &module
    );

    xrfContext.startBlock = llvm::BasicBlock::Create(context, "start", xrfContext.mainFunc);

    auto indexType = llvm::IntegerType::getInt64Ty(context);

    auto elementType = llvm::IntegerType::getInt32Ty(context);

    llvm::IRBuilder builder(xrfContext.startBlock);

    xrfContext.stackTop = builder.CreateAlloca(indexType, nullptr, "top");

    xrfContext.stackBottom = builder.CreateAlloca(indexType, nullptr, "bottom");

    xrfContext.topValue = builder.CreateAlloca(elementType, nullptr, "top_value");

    builder.CreateStore(llvm::ConstantInt::get(indexType, 0), xrfContext.stackTop);

    builder.CreateStore(llvm::ConstantInt::get(indexType, STACK_SIZE - 1), xrfContext.stackBottom);

    builder.CreateStore(llvm::ConstantInt::get(elementType, 0), xrfContext.topValue);

    return xrfContext;
}

llvm::BasicBlock *createStackJump(llvm::LLVMContext &context, XrfContext &xrfContext, const std::vector<llvm::BasicBlock*> chunks) {
    auto stackJump = llvm::BasicBlock::Create(context, "stack-jump", xrfContext.mainFunc);

    auto errorBlock = llvm::BasicBlock::Create(context, "stack-error", xrfContext.mainFunc);

    llvm::IRBuilder errorBuilder(errorBlock);

    errorBuilder.CreateUnreachable();

    llvm::IRBuilder builder(stackJump);

    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    auto switchInst = builder.CreateSwitch(topValue, errorBlock, chunks.size());

    for (size_t i = 0; i < chunks.size(); i++) {
        switchInst->addCase(
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), i),
            chunks[i]
        );
    }

    return stackJump;
}

void emitAddConstant(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, int toAdd) {
    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    auto newTopValue = builder.CreateAdd(
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), toAdd),
        topValue
    );

    builder.CreateStore(newTopValue, xrfContext.topValue);
}

void emitBottom(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, llvm::Value *value) {
    auto stackBottom = builder.CreateLoad(
        llvm::IntegerType::getInt64Ty(context),
        xrfContext.stackBottom
    );

    auto stackPtr = builder.CreateInBoundsGEP(
        xrfContext.stack,
        {
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), 0),
            stackBottom
        }
    );

    builder.CreateStore(value, stackPtr);

    auto bottomMinusOne = builder.CreateSub(
        stackBottom,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1)
    );

    auto bottomWrapped = builder.CreateAnd(bottomMinusOne, STACK_MASK);

    builder.CreateStore(bottomWrapped, xrfContext.stackBottom);
}

llvm::Value *emitGet2ndValue(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto topIndex = builder.CreateLoad(
        llvm::IntegerType::getInt64Ty(context),
        xrfContext.stackTop
    );

    auto topMinusOne = builder.CreateSub(
        topIndex,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1)
    );

    auto secondWrapped = builder.CreateAnd(topMinusOne, STACK_MASK);

    return builder.CreateInBoundsGEP(
        xrfContext.stack,
        {
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), 0),
            secondWrapped
        }
    );
}

void emitPop(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto stackTop = builder.CreateLoad(
        llvm::IntegerType::getInt64Ty(context),
        xrfContext.stackTop
    );

    auto topMinusOne = builder.CreateSub(
        stackTop,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1)
    );

    auto topWrapped = builder.CreateAnd(topMinusOne, STACK_MASK);

    auto stackPtr = builder.CreateInBoundsGEP(
        xrfContext.stack,
        {
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), 0),
            topWrapped
        }
    );

    auto newTopValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        stackPtr
    );

    builder.CreateStore(newTopValue, xrfContext.topValue);

    builder.CreateStore(topWrapped, xrfContext.stackTop);
}

void emitPush(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, llvm::Value *value) {
    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    auto stackTop = builder.CreateLoad(
        llvm::IntegerType::getInt64Ty(context),
        xrfContext.stackTop
    );

    auto stackPointer = builder.CreateInBoundsGEP(
        xrfContext.stack,
        {
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), 0),
            stackTop
        }
    );

    builder.CreateStore(topValue, stackPointer);

    builder.CreateStore(value, xrfContext.topValue);

    auto topPlusOne = builder.CreateAdd(
        stackTop,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1)
    );

    auto topWrapped = builder.CreateAnd(topPlusOne, STACK_MASK);

    builder.CreateStore(topWrapped, xrfContext.stackTop);
}

llvm::GlobalVariable *emitVisited(llvm::Module &module, llvm::LLVMContext &context, int chunkIdx) {
    auto visitedVarName = "visited-" + std::to_string(chunkIdx);

    module.getOrInsertGlobal(
        visitedVarName,
        llvm::Type::getInt1Ty(context)
    );

    auto visited = module.getGlobalVariable(visitedVarName, true);

    visited->setLinkage(llvm::GlobalValue::PrivateLinkage);
    visited->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0));

    return visited;
}

void generateAdd(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto oldTop = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    emitPop(context, xrfContext, builder);

    auto newTop = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    auto sum = builder.CreateAdd(oldTop, newTop);

    builder.CreateStore(sum, xrfContext.topValue);
}

void generateAdd2nd(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, int diff) {
    auto secondPtr = emitGet2ndValue(context, xrfContext, builder);

    auto secondValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        secondPtr
    );

    auto secondAdded = builder.CreateAdd(
        secondValue,
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), diff)
    );

    builder.CreateStore(secondAdded, secondPtr);
}

void generateBottom(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    emitPop(context, xrfContext, builder);

    emitBottom(context, xrfContext, builder, topValue);
}

void generateDec(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    emitAddConstant(context, xrfContext, builder, -1);
}

void generateDup(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    emitPush(context, xrfContext, builder, topValue);
}

void generateInc(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    emitAddConstant(context, xrfContext, builder, 1);
}

void generateInput(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto inputChar = builder.CreateCall(xrfContext.getcharFunc);

    auto isEof = builder.CreateICmpEQ(
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), -1),
        inputChar
    );

    auto correctedChar = builder.CreateSelect(
        isEof,
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), 0),
        inputChar
    );

    emitPush(context, xrfContext, builder, correctedChar);
}

void generateMultiply2nd(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, int mul) {
    auto secondPtr = emitGet2ndValue(context, xrfContext, builder);

    auto secondValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        secondPtr
    );

    auto secondMultiplied = builder.CreateNUWMul(
        secondValue,
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), mul)
    );

    builder.CreateStore(secondMultiplied, secondPtr);
}

void generateOutput(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    builder.CreateCall(xrfContext.putcharFunc, topValue);

    emitPop(context, xrfContext, builder);
}

void generatePop(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    emitPop(context, xrfContext, builder);
}

void generatePopSecond(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto topIndex = builder.CreateLoad(
        llvm::IntegerType::getInt64Ty(context),
        xrfContext.stackTop
    );

    auto topMinusOne = builder.CreateSub(
        topIndex,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1)
    );

    auto topWrapped = builder.CreateAnd(topMinusOne, STACK_MASK);

    builder.CreateStore(topWrapped, xrfContext.stackTop);
}

void generatePushSecondValue(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, unsigned val) {
    auto topIndex = builder.CreateLoad(
        llvm::IntegerType::getInt64Ty(context),
        xrfContext.stackTop
    );

    auto stackPtr = builder.CreateInBoundsGEP(
        xrfContext.stack,
        {
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), 0),
            topIndex
        }
    );

    builder.CreateStore(
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), 0),
        stackPtr
    );

    auto topPlusOne = builder.CreateAdd(
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1),
        topIndex
    );

    auto secondWrapped = builder.CreateAnd(topPlusOne, STACK_MASK);

    builder.CreateStore(secondWrapped, xrfContext.stackTop);
}

void generatePushToBottom(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, unsigned val) {
    emitBottom(
        context, xrfContext, builder,
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), val)
    );
}

void generateSetSecondValue(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, unsigned val) {
    auto secondPtr = emitGet2ndValue(context, xrfContext, builder);

    builder.CreateStore(
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), val),
        secondPtr
    );
}

void generateSetTop(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, unsigned val) {
    builder.CreateStore(
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), val),
        xrfContext.topValue
    );
}

void generateSub(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto value1 = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    emitPop(context, xrfContext, builder);

    auto value2 = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    auto sub1 = builder.CreateSub(value1, value2);

    auto sub2 = builder.CreateSub(value2, value1);

    auto value1IsGreater = builder.CreateICmpUGT(value1, value2);

    auto selectedValue = builder.CreateSelect(value1IsGreater, sub1, sub2);

    builder.CreateStore(selectedValue, xrfContext.topValue);
}

void generateSwap(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto stackTop = builder.CreateLoad(
        llvm::IntegerType::getInt64Ty(context),
        xrfContext.stackTop
    );

    auto topMinusOne = builder.CreateSub(
        stackTop,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1)
    );

    auto topWrapped = builder.CreateICmpEQ(
        stackTop,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 0)
    );

    auto stack2ndIndex = builder.CreateSelect(
        topWrapped,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), STACK_SIZE - 1),
        topMinusOne
    );

    auto stackIndex = builder.CreateInBoundsGEP(
        xrfContext.stack,
        {
            llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), 0),
            stack2ndIndex
        }
    );

    auto stack2ndValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        stackIndex
    );

    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    builder.CreateStore(topValue, stackIndex);

    builder.CreateStore(stack2ndValue, xrfContext.topValue);
}

void generateVisitJump(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, size_t index,
                       std::vector<llvm::BasicBlock*> chunks, llvm::BasicBlock *stackJump, const Chunk &visited, const Chunk &first)
{
    auto hasVisited = builder.CreateLoad(llvm::Type::getInt1Ty(context), emitVisited(*xrfContext.module, context, index));

    auto firstBlock = llvm::BasicBlock::Create(context, "", xrfContext.mainFunc);
    auto visitedBlock = llvm::BasicBlock::Create(context, "", xrfContext.mainFunc);

    generateCodeForChunk(context, xrfContext, visited, index, visitedBlock, chunks, stackJump, false);
    generateCodeForChunk(context, xrfContext, first, index, firstBlock, chunks, stackJump, true);

    builder.CreateCondBr(hasVisited, visitedBlock, firstBlock);
}

Chunk chunkFromCommand(const Chunk &chunk, size_t firstIndex) {
    Chunk shorterChunk;
    shorterChunk.commands.insert(shorterChunk.commands.begin(),
                                 chunk.commands.begin() + firstIndex,
                                 chunk.commands.end());
    shorterChunk.line = chunk.line;
    shorterChunk.col = chunk.col + firstIndex;
    return shorterChunk;
}

void generateCodeForChunk(llvm::LLVMContext &context, XrfContext &xrfContext, const Chunk &chunk, size_t index,
                          llvm::BasicBlock *chunkBlock, std::vector<llvm::BasicBlock*> chunks, llvm::BasicBlock *stackJump,
                          bool setVisited)
{
    llvm::IRBuilder builder(chunkBlock);

    for (size_t i = 0; i < chunk.commands.size(); i++) {
        const auto &command = chunk.commands[i];

        switch (command.type) {
            case CommandType::Add:
                generateAdd(context, xrfContext, builder);
                break;

            case CommandType::AddToSecond:
                generateAdd2nd(context, xrfContext, builder, command.val);
                break;

            case CommandType::Bottom:
                generateBottom(context, xrfContext, builder);
                break;

            case CommandType::Dec:
                generateDec(context, xrfContext, builder);
                break;

            case CommandType::Dup:
                generateDup(context, xrfContext, builder);
                break;

            case CommandType::Exit:
                builder.CreateRet(
                    llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), 0)
                );
                return;

            case CommandType::Inc:
                generateInc(context, xrfContext, builder);
                break;

            case CommandType::Input:
                generateInput(context, xrfContext, builder);
                break;

            case CommandType::Jump:
                // Exit this loop
                i = chunk.commands.size();
                break;

            case CommandType::MultiplySecond:
                generateMultiply2nd(context, xrfContext, builder, command.val);
                break;

            case CommandType::Output:
                generateOutput(context, xrfContext, builder);
                break;

            case CommandType::Pop:
                generatePop(context, xrfContext, builder);
                break;

            case CommandType::PopSecondValue:
                generatePopSecond(context, xrfContext, builder);
                break;

            case CommandType::PushSecondValue:
                generatePushSecondValue(context, xrfContext, builder, command.val);
                break;

            case CommandType::PushValueToBottom:
                generatePushToBottom(context, xrfContext, builder, command.val);
                break;

            case CommandType::SetSecondValue:
                generateSetSecondValue(context, xrfContext, builder, command.val);
                break;

            case CommandType::SetTop:
                generateSetTop(context, xrfContext, builder, command.val);
                break;

            case CommandType::Sub:
                generateSub(context, xrfContext, builder);
                break;

            case CommandType::Swap:
                generateSwap(context, xrfContext, builder);
                break;

            case CommandType::IgnoreVisited:
            case CommandType::IgnoreFirst: {
                if (i == chunk.commands.size() - 1) {
                    break;
                }

                auto nextChunk = chunkFromCommand(chunk, i + 1);
                auto skipChunk = chunkFromCommand(chunk, i + 2);

                if (chunk.commands[i].type == CommandType::IgnoreVisited) {
                    generateVisitJump(context, xrfContext, builder, index, chunks, stackJump, skipChunk, nextChunk);
                }
                else {
                    generateVisitJump(context, xrfContext, builder, index, chunks, stackJump, nextChunk, skipChunk);
                }

                return;
            }

            default:
                break;
        }
    }

    if (setVisited) {
        builder.CreateStore(
            llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1),
            emitVisited(*xrfContext.module, context, index)
        );
    }

    if (auto knownJump = chunk.nextChunk; knownJump) {
        builder.CreateBr(chunks[*knownJump]);
    }
    else {
        builder.CreateBr(stackJump);
    }
}

void generateChunks(llvm::LLVMContext &context, XrfContext &xrfContext, const std::vector<Chunk> &chunks) {
    std::vector<llvm::BasicBlock*> chunkStarts;

    for (size_t i = 0; i < chunks.size(); i++) {
        chunkStarts.push_back(
            llvm::BasicBlock::Create(context, "chunk" + std::to_string(i), xrfContext.mainFunc)
        );
    }

    auto *stackJump = createStackJump(context, xrfContext, chunkStarts);

    for (size_t i = 0; i < chunks.size(); i++) {
        generateCodeForChunk(context, xrfContext, chunks[i], i, chunkStarts[i], chunkStarts, stackJump);
    }

    llvm::IRBuilder builder(xrfContext.startBlock);

    builder.CreateBr(chunkStarts[0]);
}

} // anonymous namespace

std::unique_ptr<llvm::Module> generateCode(llvm::LLVMContext &context, const std::vector<Chunk> &chunks) {
    auto module = std::make_unique<llvm::Module>("xrf", context);

    auto xrfContext = getGenerationContext(*module, context);

    generateChunks(context, xrfContext, chunks);

    return module;
}

} // namespace xrf

#include "codegen.hpp"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"

namespace xrf {

namespace {

constexpr int STACK_SIZE = 65536;

struct XrfContext {
    llvm::GlobalVariable *stack;

    llvm::FunctionCallee getcharFunc;

    llvm::FunctionCallee putcharFunc;

    llvm::Function *mainFunc;

    llvm::BasicBlock *startBlock;

    llvm::AllocaInst *stackTop;

    llvm::AllocaInst *stackBottom;

    llvm::AllocaInst *topValue;
};

XrfContext getGenerationContext(llvm::Module &module, llvm::LLVMContext &context) {
    XrfContext xrfContext;

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

    auto newStackTop = builder.CreateAdd(
        stackTop,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1)
    );

    builder.CreateStore(newStackTop, xrfContext.stackTop);
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

void generateCodeForChunk(llvm::LLVMContext &context, XrfContext &xrfContext, const Chunk &chunk,
                          llvm::BasicBlock *chunkBlock, llvm::BasicBlock *stackJump)
{
    llvm::IRBuilder builder(chunkBlock);

    for (size_t i = 0; i < chunk.commands.size(); i++) {
        switch (chunk.commands[i]) {
            case CommandType::Dec:
                generateDec(context, xrfContext, builder);
                break;

            case CommandType::Dup:
                generateDup(context, xrfContext, builder);
                break;

            case CommandType::Inc:
                generateInc(context, xrfContext, builder);
                break;

            default:
                break;
        }
    }

    builder.CreateBr(stackJump);
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
        generateCodeForChunk(context, xrfContext, chunks[i], chunkStarts[i], stackJump);
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

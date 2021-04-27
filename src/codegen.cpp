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

} // anonymous namespace

std::unique_ptr<llvm::Module> generateCode(llvm::LLVMContext &context, const std::vector<Chunk> &chunks) {
    auto module = std::make_unique<llvm::Module>("xrf", context);

    auto xrfContext = getGenerationContext(*module, context);

    return module;
}

} // namespace xrf

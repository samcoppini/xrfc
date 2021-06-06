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

/**
 * A simple struct that contains various things that are needed to generate the
 * LLVM code from the XRF chunks.
 */
struct XrfContext {
    /**
     * The module that contains all of the generated LLVM code
     */
    llvm::Module *module;

    /**
     * The LLVM variable for the stack that the XRF code operates on
     */
    llvm::GlobalVariable *stack;

    /**
     * The getchar() function
     */
    llvm::FunctionCallee getcharFunc;

    /**
     * The putchar() function
     */
    llvm::FunctionCallee putcharFunc;

    /**
     * The main function where all of the XRF code is generated
     */
    llvm::Function *mainFunc;

    /**
     * The first block in the main function
     */
    llvm::BasicBlock *startBlock;

    /**
     * The index of the top value of the stack
     */
    llvm::AllocaInst *stackTop;

    /**
     * The index of the bottom value of the stack
     */
    llvm::AllocaInst *stackBottom;

    /**
     * A variable that contains the top value of the stack. This will not always
     * be equal to stack[stackTop], as the top value will only be written to the
     * stack when stack top changes
     */
    llvm::AllocaInst *topValue;
};

/**
 * Generates LLVM code for the given XRF chunk.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param chunk
 *     The XRF chunk to generate LLVM code for
 * @param index
 *     The index of the chunk in the program
 * @param chunkBlock
 *     The LLVM block to generate the code in
 * @param chunks
 *     The list of all of the blocks for the XRF chunks in the program
 * @param stackJump
 *     The block that is generated for jumping to the next chunk based on the
 *     top value of the stack
 * @param setVisited
 *     Whether to set a variable for this chunk having been visited after the
 *     execution of the chunk is finished
 */
void generateCodeForChunk(llvm::LLVMContext &context, XrfContext &xrfContext, const Chunk &chunk, size_t index,
                          llvm::BasicBlock *chunkBlock, std::vector<llvm::BasicBlock*> chunks, llvm::BasicBlock *stackJump,
                          bool setVisited = false);

/**
 * Gets the context for generating LLVM code from XRF. This sets up a number of
 * useful things for the code generation, including all of the variables the
 * XRF program will use.
 *
 * @param module
 *     The LLVM module that all of the generated code will be in
 * @param context
 *     The LLVMContext used to generate LLVM code
 *
 * @return
 *     An XRFContext with helpful fields for generating LLVM code from XRF
 */
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

/**
 * Creates a block that jumps to the next chunk to execute, as determined by
 * the top value of the stack. This block is jumped to at the end of each
 * chunk, unless the jump to this block is optimized away by determining
 * statically which chunk will be jumped to next from a given chunk.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param chunks
 *     The list of all of the LLVM blocks for the chunks
 *
 * @return
 *     The block that contains the jump based on the stack top
 */
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

/**
 * Emits LLVM code to add a constant value to the top value of the stack.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param toAdd
 *     The amount to add to the top of stack
 */
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

/**
 * Emits LLVM code to push a value to the bottom of the stack.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param value
 *     The LLVM value to push to the bottom of the stack
 */
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

/**
 * Emits LLVM code to load the second value of the stack.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 *
 * @return
 *     A pointer to the second value of the stack
 */
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

/**
 * Emits LLVM code to pop the top value of the stack.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
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

/**
 * Emits LLVM code to push a value to the stack.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param value
 *     The value to push to the stack
 */
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

/**
 * Emit a global variable that will keep track of whether a given XRF chunk has
 * been visited.
 *
 * @param module
 *     The LLVM module containing all of the generated code
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param chunkIdx
 *     The index of the chunk to keep track of
 *
 * @return
 *     The variable keeping track of whether the chunk has been visited
 */
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

/**
 * Generates LLVM code to add the top two values of the stack together.
 * Generated from the "5" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
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

/**
 * Generates LLVM code which will add a constant to the second value in the
 * stack.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param diff
 *     The amount to add to the second value of the stack
 */
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

/**
 * Generates LLVM code to send the top value of the stack to the bottom.
 * Generated from the "9" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
void generateBottom(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    emitPop(context, xrfContext, builder);

    emitBottom(context, xrfContext, builder, topValue);
}

/**
 * Generates LLVM code to decrement the top value of the stack. Generated from
 * the "6" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
void generateDec(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    emitAddConstant(context, xrfContext, builder, -1);
}

/**
 * Generates LLVM code to duplicate the top value of the stack. Generated from
 * the "3" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
void generateDup(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    emitPush(context, xrfContext, builder, topValue);
}

/**
 * Generates LLVM code to increment the top value of the stack. Generated from
 * the "5" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
void generateInc(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    emitAddConstant(context, xrfContext, builder, 1);
}

/**
 * Generates LLVM code to push a byte of input onto the stack. Generated from
 * the "0" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
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

/**
 * Generates LLVM code to multiply the second value in the stack by a constant
 * value.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param mul
 *     The amount to multiply the 2nd value by
 */
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

/**
 * Generates LLVM code to output the top value of the stack as a character,
 * popping the stack in the process. Generated from the "1" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
void generateOutput(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto topValue = builder.CreateLoad(
        llvm::IntegerType::getInt32Ty(context),
        xrfContext.topValue
    );

    builder.CreateCall(xrfContext.putcharFunc, topValue);

    emitPop(context, xrfContext, builder);
}

/**
 * Generates LLVM code to pop the stack. Generated from the "2" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
void generatePop(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    emitPop(context, xrfContext, builder);
}

/**
 * Generates LLVM code to remove the second value of the stack from the stack.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
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

/**
 * Generates LLVM code that will insert a value beneath the top value of the
 * stack.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param val
 *     The value to insert
 */
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
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), val),
        stackPtr
    );

    auto topPlusOne = builder.CreateAdd(
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1),
        topIndex
    );

    auto secondWrapped = builder.CreateAnd(topPlusOne, STACK_MASK);

    builder.CreateStore(secondWrapped, xrfContext.stackTop);
}

/**
 * Generates LLVM code that will push a value to the bottom of the stack.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param val
 *     The value to push to the bottom.
 */
void generatePushToBottom(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, unsigned val) {
    emitBottom(
        context, xrfContext, builder,
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), val)
    );
}

/**
 * Generates LLVM code which will set the second value of the stack to a known
 * value.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param val
 */
void generateSetSecondValue(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, unsigned val) {
    auto secondPtr = emitGet2ndValue(context, xrfContext, builder);

    builder.CreateStore(
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), val),
        secondPtr
    );
}

/**
 * Generates LLVM that will set the top value of the stack to a known value.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param val
 *     The value to set the top to
 */
void generateSetTop(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder, unsigned val) {
    builder.CreateStore(
        llvm::ConstantInt::get(llvm::IntegerType::getInt32Ty(context), val),
        xrfContext.topValue
    );
}

/**
 * Generates LLVM code which will replace the top two values of the stack with
 * their difference. Generated from the "E" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
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

/**
 * Generates LLVM code which will swap the top two values of the stack.
 * Generated from the "4" XRF command.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 */
void generateSwap(llvm::LLVMContext &context, XrfContext &xrfContext, llvm::IRBuilder<> &builder) {
    auto stackTop = builder.CreateLoad(
        llvm::IntegerType::getInt64Ty(context),
        xrfContext.stackTop
    );

    auto topMinusOne = builder.CreateSub(
        stackTop,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), 1)
    );

    auto stack2ndIndex = builder.CreateAnd(
        topMinusOne,
        llvm::ConstantInt::get(llvm::IntegerType::getInt64Ty(context), STACK_MASK)
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

/**
 * Generates code for conditionally executing code based on whether chunk has
 * been visited before. Generated from the "8" and "C" XRF commands.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param builder
 *     The IR builder to use for generating LLVM code
 * @param index
 *     The index of the chunk in the program
 * @param chunks
 *     The list of all of the chunks in the program
 * @param stackJump
 *     The block that is generated for jumping to the next chunk based on the
 *     top value of the stack
 * @param visited
 *     The chunk to execute if the chunk has been visited already
 * @param first
 *     The chunk to execute if the chunk has never been visited
 */
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

/**
 * Creates a new XRF chunk from a given chunk, including only the commands
 * after a specific index.
 *
 * @param chunk
 *     The chunk to take commands from.
 * @param firstIndex
 *     The index of the first command to copy from the chunk
 *
 * @return
 *     A shorter chunk made from the commands from the original chunk
 */
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

/**
 * Generates LLVM code from the XRF chunks. This function generates the bulk of
 * the LLVM IR, which includes the LLVM blocks from the XRF chunks and the
 * switch statement that gets executed at the end of every block.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param xrfContext
 *     The XRFContext that has helpful fields for generating LLVM code from XRF
 * @param chunks
 *     The XRF chunks to turn into LLVM code
 */
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

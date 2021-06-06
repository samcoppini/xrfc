#pragma once

#include "xrf-chunk.hpp"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <memory>

namespace xrf {

/**
 * Generates LLVM IR code from an XRF program.
 *
 * @param context
 *     The LLVMContext used to generate LLVM code
 * @param chunks
 *     The list of chunks that make up the XRF program
 *
 * @return
 *     An module that contains the LLVM code generated from the chunks
 */
std::unique_ptr<llvm::Module> generateCode(llvm::LLVMContext &context, const std::vector<Chunk> &chunks);

} // namespace xrf

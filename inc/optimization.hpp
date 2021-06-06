#pragma once

#include "xrf-chunk.hpp"

namespace xrf {

/**
 * Performs chunk-level optimizations on the provided list of chunks. This
 * looks at each chunk individually and tries to optimize it as best as it can,
 * by performing optimizations like eliminating unnecessary swap operations, or
 * setting a known jump location for the chunk if it is known.
 *
 * @param chunks
 *     The list of chunks to optimize.
 *
 * @return
 *     An optimized version of the chunks.
 */
std::vector<Chunk> optimizeChunks(const std::vector<Chunk> &chunks);

/**
 * Performs program-level optimizations on the provided list of chunks. This
 * will do things to look at flows of chunks to condense chunks. So, for
 * instance, if there is a series of five chunks that each add 2 to the second
 * value of the stack, it will condense it to add 10 to the second value of the
 * stack and jump to the chunk following those chunks.
 *
 * @param chunks
 *     The list of chunks to optimize. It is expected that these chunks have
 *     already gone through chunk-level optimizations, otherwise the program
 *     level optimizations will not work.
 *
 * @return
 *     An optimized version of the chunks.
 */
std::vector<Chunk> optimizeProgram(const std::vector<Chunk> &chunks);

} // namespace xrf

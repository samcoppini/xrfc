#pragma once

#include "xrf-chunk.hpp"

namespace xrf {

std::vector<Chunk> optimizeChunks(const std::vector<Chunk> &chunks);

std::vector<Chunk> optimizeProgram(const std::vector<Chunk> &chunks);

} // namespace xrf

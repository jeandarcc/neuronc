#pragma once

#include "LspTypes.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace neuron::lsp::detail {

std::vector<std::size_t> computeLineOffsets(const std::string &text);
std::pair<std::size_t, std::size_t> lineOffsetRange(const std::string &text,
                                                    int oneBasedLine);
std::string_view lineTextView(const std::string &text, int oneBasedLine);
std::string detectLineEnding(const std::string &text);
std::size_t positionToOffset(const std::string &text, int line, int character);
frontend::Position offsetToPosition(const std::string &text, std::size_t offset);
ChunkSpan buildChunkSpan(const std::string &text, std::size_t startOffset,
                         std::size_t endOffset);
std::vector<ChunkSpan> splitTopLevelChunks(const std::string &text);

} // namespace neuron::lsp::detail

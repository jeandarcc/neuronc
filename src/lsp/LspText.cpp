#include "LspText.h"

#include <algorithm>
#include <cctype>

namespace neuron::lsp::detail {

std::vector<std::size_t> computeLineOffsets(const std::string &text) {
  std::vector<std::size_t> offsets;
  offsets.push_back(0);
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\n') {
      offsets.push_back(i + 1);
    }
  }
  return offsets;
}

std::pair<std::size_t, std::size_t> lineOffsetRange(const std::string &text,
                                                    int oneBasedLine) {
  const std::vector<std::size_t> offsets = computeLineOffsets(text);
  if (oneBasedLine <= 0) {
    return {0, 0};
  }

  const std::size_t lineIndex = static_cast<std::size_t>(oneBasedLine - 1);
  if (lineIndex >= offsets.size()) {
    return {text.size(), text.size()};
  }

  const std::size_t start = offsets[lineIndex];
  const std::size_t end =
      lineIndex + 1 < offsets.size() ? offsets[lineIndex + 1] : text.size();
  return {start, end};
}

std::string_view lineTextView(const std::string &text, int oneBasedLine) {
  const auto [start, rawEnd] = lineOffsetRange(text, oneBasedLine);
  std::size_t end = rawEnd;
  while (end > start && (text[end - 1] == '\n' || text[end - 1] == '\r')) {
    --end;
  }
  return std::string_view(text).substr(start, end - start);
}

std::string detectLineEnding(const std::string &text) {
  return text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
}

std::size_t positionToOffset(const std::string &text, int line, int character) {
  const std::vector<std::size_t> offsets = computeLineOffsets(text);
  if (line < 0) {
    return 0;
  }

  const std::size_t lineIndex = static_cast<std::size_t>(line);
  if (lineIndex >= offsets.size()) {
    return text.size();
  }

  const std::size_t start = offsets[lineIndex];
  std::size_t end = text.size();
  if (lineIndex + 1 < offsets.size()) {
    end = offsets[lineIndex + 1];
  }
  return std::min(start + static_cast<std::size_t>(std::max(0, character)),
                  end);
}

frontend::Position offsetToPosition(const std::string &text, std::size_t offset) {
  frontend::Position position{1, 1};
  const std::size_t clampedOffset = std::min(offset, text.size());
  for (std::size_t index = 0; index < clampedOffset; ++index) {
    if (text[index] == '\n') {
      ++position.line;
      position.column = 1;
    } else {
      ++position.column;
    }
  }
  return position;
}

ChunkSpan buildChunkSpan(const std::string &text, std::size_t startOffset,
                         std::size_t endOffset) {
  ChunkSpan span;
  span.startOffset = startOffset;
  span.endOffset = endOffset;

  int line = 1;
  int column = 1;
  for (std::size_t i = 0; i < startOffset && i < text.size(); ++i) {
    if (text[i] == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }
  }
  span.startLine = line;
  span.startColumn = column;
  return span;
}

std::vector<ChunkSpan> splitTopLevelChunks(const std::string &text) {
  std::vector<ChunkSpan> spans;
  std::size_t chunkStart = std::string::npos;
  int braceDepth = 0;
  bool inString = false;
  bool inLineComment = false;
  bool inBlockComment = false;
  bool escape = false;

  const auto emitChunk = [&](std::size_t start, std::size_t end) {
    if (start == std::string::npos || end <= start) {
      return;
    }
    spans.push_back(buildChunkSpan(text, start, end));
  };

  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    const char next = i + 1 < text.size() ? text[i + 1] : '\0';

    if (inLineComment) {
      if (ch == '\n') {
        inLineComment = false;
      }
      continue;
    }
    if (inBlockComment) {
      if (ch == '*' && next == '/') {
        inBlockComment = false;
        ++i;
      }
      continue;
    }
    if (inString) {
      if (escape) {
        escape = false;
      } else if (ch == '\\') {
        escape = true;
      } else if (ch == '"') {
        inString = false;
      }
      continue;
    }

    if (chunkStart == std::string::npos) {
      if (std::isspace(static_cast<unsigned char>(ch))) {
        continue;
      }
      chunkStart = i;
    }

    if (ch == '/' && next == '/') {
      inLineComment = true;
      ++i;
      continue;
    }
    if (ch == '/' && next == '*') {
      inBlockComment = true;
      ++i;
      continue;
    }
    if (ch == '"') {
      inString = true;
      continue;
    }

    if (ch == '{') {
      ++braceDepth;
      continue;
    }
    if (ch == '}') {
      if (braceDepth > 0) {
        --braceDepth;
      }
      if (braceDepth == 0) {
        std::size_t end = i + 1;
        std::size_t cursor = end;
        while (cursor < text.size() &&
               (text[cursor] == ' ' || text[cursor] == '\t' ||
                text[cursor] == '\r' || text[cursor] == '\n')) {
          ++cursor;
        }
        if (cursor < text.size() && text[cursor] == ';') {
          end = cursor + 1;
        }
        emitChunk(chunkStart, end);
        chunkStart = std::string::npos;
      }
      continue;
    }

    if (braceDepth == 0 && ch == ';') {
      emitChunk(chunkStart, i + 1);
      chunkStart = std::string::npos;
    }
  }

  if (chunkStart != std::string::npos && chunkStart < text.size()) {
    emitChunk(chunkStart, text.size());
  }

  return spans;
}

} // namespace neuron::lsp::detail

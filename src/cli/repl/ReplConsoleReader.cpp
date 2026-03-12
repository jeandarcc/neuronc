#include "neuronc/cli/repl/ReplConsoleReader.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace neuron::cli {

namespace {

#ifdef _WIN32
bool isHighSurrogate(wchar_t ch) { return ch >= 0xD800 && ch <= 0xDBFF; }
bool isLowSurrogate(wchar_t ch) { return ch >= 0xDC00 && ch <= 0xDFFF; }

std::string utf8FromWideString(std::wstring_view text) {
  if (text.empty()) {
    return std::string();
  }

  const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                       static_cast<int>(text.size()), nullptr, 0,
                                       nullptr, nullptr);
  if (size <= 0) {
    return std::string();
  }

  std::string utf8(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      utf8.data(), size, nullptr, nullptr);
  return utf8;
}

std::wstring wideStringFromUtf8(std::string_view text) {
  if (text.empty()) {
    return std::wstring();
  }

  const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                       static_cast<int>(text.size()), nullptr, 0);
  if (size <= 0) {
    return std::wstring();
  }

  std::wstring wide(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      wide.data(), size);
  return wide;
}

std::size_t previousWideCodepointStart(const std::wstring &text,
                                       std::size_t index) {
  if (index == 0) {
    return 0;
  }

  std::size_t cursor = index - 1;
  if (cursor > 0 && cursor < text.size() && isLowSurrogate(text[cursor]) &&
      isHighSurrogate(text[cursor - 1])) {
    --cursor;
  }
  return cursor;
}

std::size_t nextWideCodepointEnd(const std::wstring &text, std::size_t index) {
  if (index >= text.size()) {
    return text.size();
  }

  std::size_t cursor = index + 1;
  if ((index + 1) < text.size() && isHighSurrogate(text[index]) &&
      isLowSurrogate(text[index + 1])) {
    ++cursor;
  }
  return cursor;
}

std::wstring keyTextFromEvent(const KEY_EVENT_RECORD &key) {
  if (key.uChar.UnicodeChar != 0) {
    return std::wstring(1, key.uChar.UnicodeChar);
  }

  BYTE keyboardState[256] = {};
  if (!GetKeyboardState(keyboardState)) {
    return std::wstring();
  }

  wchar_t translated[8] = {};
  const HKL layout = GetKeyboardLayout(0);
  const int count =
      ToUnicodeEx(key.wVirtualKeyCode, key.wVirtualScanCode, keyboardState,
                  translated, static_cast<int>(std::size(translated)), 0, layout);
  if (count <= 0) {
    return std::wstring();
  }
  return std::wstring(translated, translated + count);
}

std::vector<std::wstring_view> splitDisplayLines(std::wstring_view text) {
  std::vector<std::wstring_view> lines;
  if (text.empty()) {
    lines.push_back(std::wstring_view{});
    return lines;
  }

  std::size_t lineStart = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != L'\n') {
      continue;
    }
    lines.push_back(text.substr(lineStart, i - lineStart));
    lineStart = i + 1;
  }
  lines.push_back(text.substr(lineStart));
  return lines;
}

std::size_t currentLineStart(const std::wstring &buffer, std::size_t cursorIndex) {
  const std::size_t safeIndex = std::min(cursorIndex, buffer.size());
  if (safeIndex == 0) {
    return 0;
  }
  const std::size_t lineBreak = buffer.rfind(L'\n', safeIndex - 1);
  return lineBreak == std::wstring::npos ? 0 : lineBreak + 1;
}

std::size_t currentLineEnd(const std::wstring &buffer, std::size_t cursorIndex) {
  const std::size_t safeIndex = std::min(cursorIndex, buffer.size());
  const std::size_t lineBreak = buffer.find(L'\n', safeIndex);
  return lineBreak == std::wstring::npos ? buffer.size() : lineBreak;
}

bool isSingleLineBuffer(const std::wstring &buffer) {
  return buffer.find(L'\n') == std::wstring::npos;
}

void writeConsoleText(HANDLE handle, std::wstring_view text) {
  if (text.empty()) {
    return;
  }

  DWORD written = 0;
  WriteConsoleW(handle, text.data(), static_cast<DWORD>(text.size()), &written,
                nullptr);
}

void moveConsoleCursor(HANDLE handle, SHORT x, SHORT y) {
  const COORD coord{x, y};
  SetConsoleCursorPosition(handle, coord);
}

void eraseConsoleCellAtCursor(HANDLE handle) {
  CONSOLE_SCREEN_BUFFER_INFO info{};
  if (!GetConsoleScreenBufferInfo(handle, &info)) {
    return;
  }
  if (info.dwCursorPosition.X <= 0) {
    return;
  }

  const SHORT newX = static_cast<SHORT>(info.dwCursorPosition.X - 1);
  const SHORT y = info.dwCursorPosition.Y;
  moveConsoleCursor(handle, newX, y);
  DWORD written = 0;
  const COORD coord{newX, y};
  FillConsoleOutputCharacterW(handle, L' ', 1, coord, &written);
  FillConsoleOutputAttribute(handle, info.wAttributes, 1, coord, &written);
  moveConsoleCursor(handle, newX, y);
}

void clearConsoleLine(HANDLE handle, SHORT x, SHORT y, SHORT width,
                      WORD attributes) {
  DWORD written = 0;
  const COORD lineStart{x, y};
  FillConsoleOutputCharacterW(handle, L' ', static_cast<DWORD>(width), lineStart,
                              &written);
  FillConsoleOutputAttribute(handle, attributes, static_cast<DWORD>(width),
                             lineStart, &written);
}

void positionCursorForBuffer(HANDLE handle, const COORD &origin,
                             std::wstring_view prompt,
                             std::wstring_view continuationPrompt,
                             const std::wstring &buffer,
                             std::size_t cursorIndex) {
  const std::size_t safeCursorIndex = std::min(cursorIndex, buffer.size());
  std::size_t cursorLine = 0;
  std::size_t cursorColumn = 0;
  std::size_t i = 0;
  while (i < safeCursorIndex) {
    if (buffer[i] == L'\n') {
      ++cursorLine;
      cursorColumn = 0;
      ++i;
      continue;
    }
    ++cursorColumn;
    i = nextWideCodepointEnd(buffer, i);
  }

  const SHORT cursorX = static_cast<SHORT>(
      origin.X + (cursorLine == 0 ? prompt.size() : continuationPrompt.size()) +
      cursorColumn);
  const SHORT cursorY = static_cast<SHORT>(origin.Y + cursorLine);
  moveConsoleCursor(handle, cursorX, cursorY);
}

void renderConsoleBuffer(HANDLE handle, const COORD &origin, SHORT consoleWidth,
                         WORD attributes, std::wstring_view prompt,
                         std::wstring_view continuationPrompt,
                         const std::wstring &buffer, std::size_t cursorIndex,
                         SHORT *ioRenderedLineCount) {
  const std::vector<std::wstring_view> lines = splitDisplayLines(buffer);
  const SHORT newRenderedLineCount =
      static_cast<SHORT>(std::max<std::size_t>(1, lines.size()));
  const SHORT oldRenderedLineCount =
      ioRenderedLineCount != nullptr ? *ioRenderedLineCount : 0;
  const SHORT clearLineCount =
      std::max(oldRenderedLineCount, newRenderedLineCount);
  const SHORT clearWidth =
      static_cast<SHORT>(std::max<int>(1, consoleWidth - origin.X));

  for (SHORT i = 0; i < clearLineCount; ++i) {
    clearConsoleLine(handle, origin.X, static_cast<SHORT>(origin.Y + i),
                     clearWidth, attributes);
  }

  for (SHORT i = 0; i < newRenderedLineCount; ++i) {
    moveConsoleCursor(handle, origin.X, static_cast<SHORT>(origin.Y + i));
    writeConsoleText(handle, i == 0 ? prompt : continuationPrompt);
    if (static_cast<std::size_t>(i) < lines.size()) {
      writeConsoleText(handle, lines[static_cast<std::size_t>(i)]);
    }
  }

  positionCursorForBuffer(handle, origin, prompt, continuationPrompt, buffer,
                          cursorIndex);
  if (ioRenderedLineCount != nullptr) {
    *ioRenderedLineCount = newRenderedLineCount;
  }
}

void rewriteSingleLineTail(HANDLE handle, const COORD &origin,
                           std::wstring_view prompt,
                           std::wstring_view continuationPrompt,
                           const std::wstring &buffer, std::size_t writeFromIndex,
                           std::size_t cursorIndex, bool clearTrailingCell) {
  if (!isSingleLineBuffer(buffer)) {
    return;
  }

  const std::size_t safeWriteFrom = std::min(writeFromIndex, buffer.size());
  positionCursorForBuffer(handle, origin, prompt, continuationPrompt, buffer,
                          safeWriteFrom);
  writeConsoleText(handle,
                   std::wstring_view(buffer).substr(
                       safeWriteFrom, buffer.size() - safeWriteFrom));
  if (clearTrailingCell) {
    writeConsoleText(handle, L" ");
  }
  positionCursorForBuffer(handle, origin, prompt, continuationPrompt, buffer,
                          cursorIndex);
}
#endif

} // namespace

void ReplConsoleReader::remember(std::string text) {
  if (text.empty()) {
    return;
  }
  m_history.push_back(std::move(text));
}

bool ReplConsoleReader::isInteractiveInput() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

ReplReadResult ReplConsoleReader::read(const std::string &prompt,
                                       const std::string &continuationPrompt) {
  ReplReadResult result;

#ifdef _WIN32
  if (isInteractiveInput()) {
    HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD originalMode = 0;
    if (inputHandle != INVALID_HANDLE_VALUE &&
        outputHandle != INVALID_HANDLE_VALUE &&
        GetConsoleMode(inputHandle, &originalMode)) {
      DWORD rawMode = originalMode;
      rawMode |= ENABLE_EXTENDED_FLAGS;
      rawMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

      if (SetConsoleMode(inputHandle, rawMode)) {
        const std::wstring promptWide = wideStringFromUtf8(prompt);
        const std::wstring continuationPromptWide =
            wideStringFromUtf8(continuationPrompt);
        std::wstring buffer;
        std::size_t cursorIndex = 0;
        std::optional<std::size_t> historyIndex;
        std::wstring historyDraft;

        CONSOLE_SCREEN_BUFFER_INFO outputInfo{};
        if (!GetConsoleScreenBufferInfo(outputHandle, &outputInfo)) {
          SetConsoleMode(inputHandle, originalMode);
          return result;
        }
        const COORD promptOrigin{0, outputInfo.dwCursorPosition.Y};
        SHORT renderedLineCount = 0;
        renderConsoleBuffer(outputHandle, promptOrigin, outputInfo.dwSize.X,
                            outputInfo.wAttributes, promptWide,
                            continuationPromptWide, buffer, cursorIndex,
                            &renderedLineCount);

        auto exitHistoryBrowse = [&]() {
          historyIndex.reset();
          historyDraft.clear();
        };

        for (;;) {
          INPUT_RECORD record{};
          DWORD readCount = 0;
          if (!ReadConsoleInputW(inputHandle, &record, 1, &readCount) ||
              readCount == 0) {
            result.eof = true;
            break;
          }
          if (record.EventType != KEY_EVENT ||
              !record.Event.KeyEvent.bKeyDown) {
            continue;
          }

          const KEY_EVENT_RECORD &key = record.Event.KeyEvent;
          const bool controlDown =
              (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) !=
              0;
          const bool altDown =
              (key.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
          const bool plainCtrlDown = controlDown && !altDown;
          const auto refresh = [&]() {
            CONSOLE_SCREEN_BUFFER_INFO refreshInfo{};
            GetConsoleScreenBufferInfo(outputHandle, &refreshInfo);
            renderConsoleBuffer(outputHandle, promptOrigin, refreshInfo.dwSize.X,
                                refreshInfo.wAttributes, promptWide,
                                continuationPromptWide, buffer, cursorIndex,
                                &renderedLineCount);
          };
          const auto moveCursorOnly = [&]() {
            positionCursorForBuffer(outputHandle, promptOrigin, promptWide,
                                    continuationPromptWide, buffer,
                                    cursorIndex);
          };

          if (key.wVirtualKeyCode == VK_RETURN) {
            if (plainCtrlDown) {
              exitHistoryBrowse();
              buffer.insert(cursorIndex, 1, L'\n');
              ++cursorIndex;
              refresh();
              continue;
            }
            cursorIndex = buffer.size();
            moveCursorOnly();
            writeConsoleText(outputHandle, L"\r\n");
            result.text = utf8FromWideString(buffer);
            break;
          }

          if (key.wVirtualKeyCode == VK_LEFT) {
            cursorIndex = previousWideCodepointStart(buffer, cursorIndex);
            moveCursorOnly();
            continue;
          }

          if (key.wVirtualKeyCode == VK_RIGHT) {
            cursorIndex = nextWideCodepointEnd(buffer, cursorIndex);
            moveCursorOnly();
            continue;
          }

          if (key.wVirtualKeyCode == VK_UP) {
            if (m_history.empty()) {
              continue;
            }
            if (!historyIndex.has_value()) {
              historyDraft = buffer;
              historyIndex = m_history.size() - 1;
            } else if (*historyIndex > 0) {
              --(*historyIndex);
            }
            buffer = wideStringFromUtf8(m_history[*historyIndex]);
            cursorIndex = buffer.size();
            refresh();
            continue;
          }

          if (key.wVirtualKeyCode == VK_DOWN) {
            if (!historyIndex.has_value()) {
              continue;
            }
            if ((*historyIndex + 1) < m_history.size()) {
              ++(*historyIndex);
              buffer = wideStringFromUtf8(m_history[*historyIndex]);
            } else {
              buffer = historyDraft;
              historyIndex.reset();
              historyDraft.clear();
            }
            cursorIndex = buffer.size();
            refresh();
            continue;
          }

          if (key.wVirtualKeyCode == VK_HOME) {
            cursorIndex = currentLineStart(buffer, cursorIndex);
            moveCursorOnly();
            continue;
          }

          if (key.wVirtualKeyCode == VK_END) {
            cursorIndex = currentLineEnd(buffer, cursorIndex);
            moveCursorOnly();
            continue;
          }

          if (key.wVirtualKeyCode == VK_BACK) {
            if (cursorIndex > 0) {
              exitHistoryBrowse();
              const std::size_t eraseStart =
                  previousWideCodepointStart(buffer, cursorIndex);
              const bool eraseAtEnd = cursorIndex == buffer.size() &&
                                      eraseStart < buffer.size() &&
                                      buffer[eraseStart] != L'\n';
              const bool eraseInSingleLineMiddle =
                  !eraseAtEnd && isSingleLineBuffer(buffer) &&
                  buffer[eraseStart] != L'\n';
              buffer.erase(eraseStart, cursorIndex - eraseStart);
              cursorIndex = eraseStart;
              if (eraseAtEnd) {
                eraseConsoleCellAtCursor(outputHandle);
              } else if (eraseInSingleLineMiddle) {
                rewriteSingleLineTail(outputHandle, promptOrigin, promptWide,
                                      continuationPromptWide, buffer, eraseStart,
                                      cursorIndex, true);
              } else {
                refresh();
              }
            }
            continue;
          }

          if (key.wVirtualKeyCode == VK_DELETE) {
            if (cursorIndex < buffer.size()) {
              exitHistoryBrowse();
              const std::size_t eraseEnd =
                  nextWideCodepointEnd(buffer, cursorIndex);
              const bool eraseInSingleLineMiddle =
                  isSingleLineBuffer(buffer) && buffer[cursorIndex] != L'\n';
              buffer.erase(cursorIndex, eraseEnd - cursorIndex);
              if (eraseInSingleLineMiddle) {
                rewriteSingleLineTail(outputHandle, promptOrigin, promptWide,
                                      continuationPromptWide, buffer, cursorIndex,
                                      cursorIndex, true);
              } else {
                refresh();
              }
            }
            continue;
          }

          if (key.wVirtualKeyCode == VK_ESCAPE && buffer.empty()) {
            writeConsoleText(outputHandle, L"\r\n");
            result.eof = true;
            break;
          }

          if (plainCtrlDown) {
            continue;
          }

          const std::wstring keyText = keyTextFromEvent(key);
          if (keyText.empty()) {
            continue;
          }

          exitHistoryBrowse();
          const std::size_t insertStart = cursorIndex;
          const bool appendAtEnd = cursorIndex == buffer.size();
          const bool insertInSingleLineMiddle =
              !appendAtEnd && isSingleLineBuffer(buffer);
          buffer.insert(cursorIndex, keyText);
          cursorIndex += keyText.size();
          if (appendAtEnd) {
            writeConsoleText(outputHandle, keyText);
          } else if (insertInSingleLineMiddle) {
            rewriteSingleLineTail(outputHandle, promptOrigin, promptWide,
                                  continuationPromptWide, buffer, insertStart,
                                  cursorIndex, false);
          } else {
            refresh();
          }
        }

        SetConsoleMode(inputHandle, originalMode);
        return result;
      }
    }
  }
#endif

  std::cout << prompt;
  std::cout.flush();
  if (!std::getline(std::cin, result.text)) {
    result.eof = true;
  }
  (void)continuationPrompt;
  return result;
}

} // namespace neuron::cli

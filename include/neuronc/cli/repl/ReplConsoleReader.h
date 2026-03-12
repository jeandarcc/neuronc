#pragma once

#include <string>
#include <vector>

namespace neuron::cli {

struct ReplReadResult {
  bool eof = false;
  std::string text;
};

class ReplConsoleReader {
public:
  ReplReadResult read(const std::string &prompt,
                      const std::string &continuationPrompt = "... ");
  void remember(std::string text);

  static bool isInteractiveInput();

private:
  std::vector<std::string> m_history;
};

} // namespace neuron::cli

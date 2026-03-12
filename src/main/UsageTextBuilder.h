#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace neuron::cli {

struct HelpEntry {
  int order = 0;
  std::string command;
  std::string detail;
  bool multiline = false;
};

struct HelpSection {
  int order = 0;
  std::string header;
  std::vector<std::string> lines;
  std::vector<HelpEntry> entries;
};

struct HelpDocument {
  std::string title;
  std::string version;
  std::vector<HelpSection> sections;
};

std::optional<HelpDocument> loadHelpDocumentFromToml(
    const std::filesystem::path &path);
std::string renderHelpDocument(const HelpDocument &document);
std::string buildUsageText(const std::filesystem::path &toolRoot);

} // namespace neuron::cli


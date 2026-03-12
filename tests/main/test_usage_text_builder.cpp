#include "../../src/main/UsageText.h"
#include "../../src/main/UsageTextBuilder.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

struct ScopedHelpTomlRoot {
  fs::path root;

  ScopedHelpTomlRoot() {
    root = fs::temp_directory_path() / "npp_usage_text_builder_tests";
    fs::remove_all(root);
    fs::create_directories(root / "config" / "cli");
  }

  ~ScopedHelpTomlRoot() { fs::remove_all(root); }
};

} // namespace

TEST(UsageTextBuilderLoadsTomlAndRendersOrderedSections) {
  ScopedHelpTomlRoot scoped;
  const fs::path helpTomlPath = scoped.root / "config" / "cli" / "help.toml";

  std::ofstream out(helpTomlPath);
  out << "title = \"test\"\n";
  out << "version = \"1\"\n\n";
  out << "[[section]]\n";
  out << "order = 20\n";
  out << "header = \"Second:\"\n";
  out << "\n";
  out << "[[section.entry]]\n";
  out << "order = 20\n";
  out << "command = \"beta\"\n";
  out << "detail = \"second entry\"\n";
  out << "\n";
  out << "[[section.entry]]\n";
  out << "order = 10\n";
  out << "command = \"alpha\"\n";
  out << "detail = \"first entry\"\n";
  out << "\n";
  out << "[[section]]\n";
  out << "order = 10\n";
  out << "header = \"First:\"\n";
  out << "lines = [\"  banner line\", \"\"]\n";
  out.close();

  const auto document = neuron::cli::loadHelpDocumentFromToml(helpTomlPath);
  ASSERT_TRUE(document.has_value());
  ASSERT_EQ(document->sections.size(), static_cast<std::size_t>(2));
  ASSERT_EQ(document->sections[0].header, "First:");
  ASSERT_EQ(document->sections[1].header, "Second:");
  ASSERT_EQ(document->sections[1].entries[0].command, "alpha");
  ASSERT_EQ(document->sections[1].entries[1].command, "beta");

  const std::string rendered = neuron::cli::renderHelpDocument(*document);
  ASSERT_TRUE(rendered.find("  First:\n  banner line") != std::string::npos);
  ASSERT_TRUE(rendered.find("    alpha") != std::string::npos);
  ASSERT_TRUE(rendered.find("first entry") != std::string::npos);
  return true;
}

TEST(UsageTextBuilderFallsBackToEmbeddedUsageWhenTomlMissing) {
  const std::string usage = neuron::cli::buildUsageText(
      fs::temp_directory_path() / "npp_usage_text_builder_missing_root");
  ASSERT_EQ(usage, std::string(neuron::cli::kUsageText));
  return true;
}


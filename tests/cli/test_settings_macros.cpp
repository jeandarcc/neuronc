#include "neuronc/cli/SettingsMacros.h"
#include "neuronc/lexer/Lexer.h"
#include "neuronc/parser/Parser.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace neuron;
namespace fs = std::filesystem;

namespace {

void removeAllNoThrow(const fs::path &path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return;
  }
  for (fs::recursive_directory_iterator it(path, ec), end; it != end;
       it.increment(ec)) {
    if (ec) {
      break;
    }
    fs::permissions(it->path(), fs::perms::owner_all, fs::perm_options::add, ec);
  }
  ec.clear();
  fs::permissions(path, fs::perms::owner_all, fs::perm_options::add, ec);
  ec.clear();
  fs::remove_all(path, ec);
}

struct ScopedProjectDir {
  explicit ScopedProjectDir(const std::string &name)
      : path(fs::current_path() / name) {
    removeAllNoThrow(path);
  }

  ScopedProjectDir(const ScopedProjectDir &) = delete;
  ScopedProjectDir &operator=(const ScopedProjectDir &) = delete;

  ~ScopedProjectDir() {
    if (!path.empty()) {
      removeAllNoThrow(path);
    }
  }

  fs::path path;
};

fs::path repoRootPath() {
  return fs::path(__FILE__).parent_path().parent_path().parent_path();
}

void writeTextFile(const fs::path &path, const std::string &text) {
  if (!path.parent_path().empty()) {
    fs::create_directories(path.parent_path());
  }
  std::ofstream out(path, std::ios::binary);
  out << text;
}

void writeProjectToml(const fs::path &root, const std::string &name,
                      const std::string &mainFile) {
  writeTextFile(root / "neuron.toml",
                "[project]\n"
                "name = \"" + name + "\"\n"
                "version = \"0.1.0\"\n\n"
                "[build]\n"
                "main = \"" + mainFile + "\"\n");
}

std::vector<Token> lexOrEmpty(const std::string &source, const fs::path &path,
                              std::vector<std::string> *outErrors) {
  Lexer lexer(source, path.string());
  auto tokens = lexer.tokenize();
  if (!lexer.errors().empty() && outErrors != nullptr) {
    *outErrors = lexer.errors();
  }
  return tokens;
}

bool expandSource(neuron::cli::SettingsMacroProcessor *processor,
                  const fs::path &path, const std::string &source,
                  std::vector<Token> *outTokens,
                  std::vector<std::string> *outErrors) {
  const auto tokens = lexOrEmpty(source, path, outErrors);
  if (outErrors != nullptr && !outErrors->empty()) {
    return false;
  }
  return processor->expandSourceTokens(path, tokens, outTokens, outErrors);
}

bool expandSourceWithTrace(neuron::cli::SettingsMacroProcessor *processor,
                          const fs::path &path, const std::string &source,
                          std::vector<Token> *outTokens,
                          std::vector<neuron::cli::MacroExpansionTrace> *outTraces,
                          std::vector<std::string> *outErrors) {
  const auto tokens = lexOrEmpty(source, path, outErrors);
  if (outErrors != nullptr && !outErrors->empty()) {
    return false;
  }
  return processor->expandSourceTokensWithTrace(path, tokens, outTokens, outTraces,
                                                outErrors);
}

bool containsTokenValue(const std::vector<Token> &tokens, const std::string &value) {
  for (const auto &token : tokens) {
    if (token.value == value) {
      return true;
    }
  }
  return false;
}

void printErrors(const std::vector<std::string> &errors) {
  for (const auto &error : errors) {
    std::cerr << error << std::endl;
  }
}

} // namespace

TEST(SettingsMacroExpandsQualifiedMethodMacroWithComments) {
  ScopedProjectDir project("tmp_settings_macro_method");
  writeProjectToml(project.path, "macro_method", "src/Main.nr");
  writeTextFile(project.path / "src/Main.nr",
                "Init is method() {\n"
                "  value is Main.PRINT_COPYRIGHT();\n"
                "}\n");
  writeTextFile(project.path / ".modulesettings",
                "# leading comment\n"
                "[Main] // section comment\n"
                "PRINT_COPYRIGHT = method() {\n"
                "  # body comment\n"
                "  return 7;\n"
                "}\n");

  std::vector<std::string> errors;
  neuron::cli::SettingsMacroProcessor processor(repoRootPath(),
                                                project.path / "src/Main.nr");
  const bool initOk = processor.initialize(&errors);
  if (!initOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(initOk);

  std::vector<Token> expanded;
  const std::string source =
      "Init is method() {\n"
      "  value is Main.PRINT_COPYRIGHT();\n"
      "}\n";
  const bool expandOk = expandSource(&processor, project.path / "src/Main.nr",
                                     source, &expanded, &errors);
  if (!expandOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(expandOk);
  ASSERT_TRUE(containsTokenValue(expanded, "method"));
  ASSERT_TRUE(containsTokenValue(expanded, "7"));

  Parser parser(expanded, (project.path / "src/Main.nr").string());
  auto ast = parser.parse();
  (void)ast;
  if (!parser.errors().empty()) {
    printErrors(parser.errors());
  }
  ASSERT_TRUE(parser.errors().empty());
  return true;
}

TEST(SettingsMacroReportsExpansionTraceForQualifiedUse) {
  ScopedProjectDir project("tmp_settings_macro_trace");
  writeProjectToml(project.path, "macro_trace", "src/Main.nr");
  writeTextFile(project.path / "src/Main.nr",
                "Init is method() {\n"
                "  value is Main.PORT;\n"
                "}\n");
  writeTextFile(project.path / ".modulesettings",
                "[Main]\n"
                "PORT = 7001;\n");

  std::vector<std::string> errors;
  neuron::cli::SettingsMacroProcessor processor(repoRootPath(),
                                                project.path / "src/Main.nr");
  ASSERT_TRUE(processor.initialize(&errors));

  std::vector<Token> expanded;
  std::vector<neuron::cli::MacroExpansionTrace> traces;
  const bool expandOk = expandSourceWithTrace(
      &processor, project.path / "src/Main.nr",
      "Init is method() { value is Main.PORT; }\n", &expanded, &traces, &errors);
  if (!expandOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(expandOk);
  ASSERT_EQ(traces.size(), 1u);
  ASSERT_EQ(traces.front().qualifiedName, "Main.PORT");
  ASSERT_EQ(traces.front().expansion, "7001");
  ASSERT_EQ(traces.front().useLocation.line, 1);
  ASSERT_TRUE(traces.front().useLength >= 9);
  return true;
}

TEST(SettingsMacroRejectsUnknownOverrideKey) {
  ScopedProjectDir project("tmp_settings_macro_unknown_key");
  writeProjectToml(project.path, "macro_unknown", "src/Main.nr");
  writeTextFile(project.path / "src/Main.nr", "Init is method() {}\n");
  writeTextFile(project.path / ".projectsettings",
                "[IO]\n"
                "GIVE_ME_ERROR = 1;\n");

  std::vector<std::string> errors;
  neuron::cli::SettingsMacroProcessor processor(repoRootPath(),
                                                project.path / "src/Main.nr");
  ASSERT_FALSE(processor.initialize(&errors));
  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(errors.front().find("Unknown override key 'IO.GIVE_ME_ERROR'") !=
              std::string::npos);
  return true;
}

TEST(SettingsMacroRejectsInvalidImportanceLevel) {
  ScopedProjectDir project("tmp_settings_macro_bad_importance");
  writeProjectToml(project.path, "macro_bad_importance", "src/Main.nr");
  writeTextFile(project.path / "src/Main.nr", "Init is method() {}\n");
  writeTextFile(project.path / ".projectsettings",
                "[IO]\n"
                "important(0) ENUM_INPUT_NEXT_KEY = 99;\n");

  std::vector<std::string> errors;
  neuron::cli::SettingsMacroProcessor processor(repoRootPath(),
                                                project.path / "src/Main.nr");
  ASSERT_FALSE(processor.initialize(&errors));
  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(errors.front().find("level >= 1") != std::string::npos);
  return true;
}

TEST(SettingsMacroBareLookupReportsAmbiguity) {
  ScopedProjectDir project("tmp_settings_macro_ambiguity");
  writeProjectToml(project.path, "macro_ambiguity", "src/Main.nr");
  writeTextFile(project.path / "src/Main.nr",
                "Init is method() {\n"
                "  value is VALUE;\n"
                "}\n");
  writeTextFile(project.path / "src/A.nr", "Ping is method() {}\n");
  writeTextFile(project.path / "src/B.nr", "Pong is method() {}\n");
  writeTextFile(project.path / ".modulesettings",
                "[A]\n"
                "VALUE = 1;\n"
                "[B]\n"
                "VALUE = 2;\n");

  std::vector<std::string> errors;
  neuron::cli::SettingsMacroProcessor processor(repoRootPath(),
                                                project.path / "src/Main.nr");
  const bool initOk = processor.initialize(&errors);
  if (!initOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(initOk);

  std::vector<Token> expanded;
  const bool expandOk =
      expandSource(&processor, project.path / "src/Main.nr",
                   "Init is method() { value is VALUE; }\n", &expanded, &errors);
  ASSERT_FALSE(expandOk);
  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(errors.front().find("Ambiguous macro 'VALUE'") != std::string::npos);
  ASSERT_TRUE(errors.front().find("A.VALUE") != std::string::npos);
  ASSERT_TRUE(errors.front().find("B.VALUE") != std::string::npos);
  return true;
}

TEST(SettingsMacroDependencyLocalOverrideWinsAtEqualImportance) {
  ScopedProjectDir project("tmp_settings_macro_equal_importance");
  writeProjectToml(project.path, "macro_equal", "src/Main.nr");
  writeProjectToml(project.path / "modules/dep", "dep", "src/Dep.nr");
  writeTextFile(project.path / "src/Main.nr", "Init is method() {}\n");
  writeTextFile(project.path / "modules/dep/src/Dep.nr",
                "DepValue is method() { value is ENUM_INPUT_NEXT_KEY; }\n");
  writeTextFile(project.path / ".projectsettings",
                "[IO]\n"
                "ENUM_INPUT_NEXT_KEY = 111;\n");
  writeTextFile(project.path / "modules/dep/.projectsettings",
                "[IO]\n"
                "ENUM_INPUT_NEXT_KEY = 222;\n");

  std::vector<std::string> errors;
  neuron::cli::SettingsMacroProcessor processor(repoRootPath(),
                                                project.path / "src/Main.nr");
  const bool initOk = processor.initialize(&errors);
  if (!initOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(initOk);

  std::vector<Token> expanded;
  const bool expandOk = expandSource(
      &processor, project.path / "modules/dep/src/Dep.nr",
      "DepValue is method() { value is ENUM_INPUT_NEXT_KEY; }\n", &expanded,
      &errors);
  if (!expandOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(expandOk);
  ASSERT_TRUE(containsTokenValue(expanded, "222"));
  ASSERT_FALSE(containsTokenValue(expanded, "111"));
  return true;
}

TEST(SettingsMacroHigherImportanceAncestorOverrideWins) {
  ScopedProjectDir project("tmp_settings_macro_high_importance");
  writeProjectToml(project.path, "macro_high", "src/Main.nr");
  writeProjectToml(project.path / "modules/dep", "dep", "src/Dep.nr");
  writeTextFile(project.path / "src/Main.nr", "Init is method() {}\n");
  writeTextFile(project.path / "modules/dep/src/Dep.nr",
                "DepValue is method() { value is ENUM_INPUT_NEXT_KEY; }\n");
  writeTextFile(project.path / ".projectsettings",
                "[IO]\n"
                "important(2) ENUM_INPUT_NEXT_KEY = 333;\n");
  writeTextFile(project.path / "modules/dep/.projectsettings",
                "[IO]\n"
                "important ENUM_INPUT_NEXT_KEY = 222;\n");

  std::vector<std::string> errors;
  neuron::cli::SettingsMacroProcessor processor(repoRootPath(),
                                                project.path / "src/Main.nr");
  const bool initOk = processor.initialize(&errors);
  if (!initOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(initOk);

  std::vector<Token> expanded;
  const bool expandOk = expandSource(
      &processor, project.path / "modules/dep/src/Dep.nr",
      "DepValue is method() { value is ENUM_INPUT_NEXT_KEY; }\n", &expanded,
      &errors);
  if (!expandOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(expandOk);
  ASSERT_TRUE(containsTokenValue(expanded, "333"));
  ASSERT_FALSE(containsTokenValue(expanded, "222"));
  return true;
}

TEST(SettingsMacroParentLocalOverrideDoesNotBreakDependencyExpansion) {
  ScopedProjectDir project("tmp_settings_macro_parent_local");
  writeProjectToml(project.path, "macro_parent_local", "src/Main.nr");
  writeProjectToml(project.path / "modules/dep", "dep", "src/Dep.nr");
  writeTextFile(project.path / "src/Main.nr", "Init is method() {}\n");
  writeTextFile(project.path / "modules/dep/src/Dep.nr",
                "DepValue is method() { value is ENUM_INPUT_NEXT_KEY; }\n");
  writeTextFile(project.path / ".modulesettings",
                "[Main]\n"
                "ROOT_ONLY = 1;\n");
  writeTextFile(project.path / ".projectsettings",
                "[Main]\n"
                "ROOT_ONLY = 2;\n");
  writeTextFile(project.path / "modules/dep/.projectsettings",
                "[IO]\n"
                "ENUM_INPUT_NEXT_KEY = 222;\n");

  std::vector<std::string> errors;
  neuron::cli::SettingsMacroProcessor processor(repoRootPath(),
                                                project.path / "src/Main.nr");
  const bool initOk = processor.initialize(&errors);
  if (!initOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(initOk);

  std::vector<Token> expanded;
  const bool expandOk = expandSource(
      &processor, project.path / "modules/dep/src/Dep.nr",
      "DepValue is method() { value is ENUM_INPUT_NEXT_KEY; }\n", &expanded,
      &errors);
  if (!expandOk) {
    printErrors(errors);
  }
  ASSERT_TRUE(expandOk);
  ASSERT_TRUE(containsTokenValue(expanded, "222"));
  return true;
}

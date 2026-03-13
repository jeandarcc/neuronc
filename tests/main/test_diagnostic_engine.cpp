#include "main/DiagnosticEngine.h"
#include "main/AppGlobals.h"
#include "main/UserProfileSettings.h"

#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace {

struct ScopedAppDataEnvVarForDiagnosticEngineTest {
  explicit ScopedAppDataEnvVarForDiagnosticEngineTest(const std::string &value) {
#ifdef _WIN32
    _putenv_s("APPDATA", value.c_str());
#else
    (void)value;
#endif
  }
  ~ScopedAppDataEnvVarForDiagnosticEngineTest() {
#ifdef _WIN32
    _putenv_s("APPDATA", "");
#endif
  }
};

struct ScopedStderrCapture {
  std::ostringstream stream;
  std::streambuf *old = nullptr;

  ScopedStderrCapture() : old(std::cerr.rdbuf(stream.rdbuf())) {}
  ~ScopedStderrCapture() { std::cerr.rdbuf(old); }
};

struct ScopedToolRootForDiagnosticEngineTest {
  std::filesystem::path old;

  explicit ScopedToolRootForDiagnosticEngineTest(std::filesystem::path value)
      : old(g_toolRoot) {
    g_toolRoot = std::move(value);
  }

  ~ScopedToolRootForDiagnosticEngineTest() { g_toolRoot = old; }
};

} // namespace

TEST(DiagnosticEnginePrintsLocalizedCliDiagnostics) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "neuron_diagnostic_engine_tests";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "Neuron");
  ScopedAppDataEnvVarForDiagnosticEngineTest env(root.string());
  ScopedToolRootForDiagnosticEngineTest toolRoot(
      std::filesystem::absolute(std::filesystem::path(__FILE__))
          .parent_path()
          .parent_path()
          .parent_path());

  neuron::UserProfileSettings settings;
  settings.language = "tr";
  ASSERT_TRUE(neuron::saveUserProfileSettings(settings));

  ScopedStderrCapture capture;
  reportStringDiagnostics("semantic", "demo.nr", "let x = 1;",
                          {"demo.nr:1:1: warning: Unused variable: x"});

  ASSERT_TRUE(capture.stream.str().find("Bildirilen baglama hic okunmuyor.") !=
              std::string::npos);
  ASSERT_TRUE(capture.stream.str().find("NR9002") != std::string::npos);
  std::filesystem::remove_all(root);
  return true;
}

TEST(DiagnosticEngineRendersParserTemplatesWithoutRawEnglishTail) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "neuron_diagnostic_engine_tests";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "Neuron");
  ScopedAppDataEnvVarForDiagnosticEngineTest env(root.string());
  ScopedToolRootForDiagnosticEngineTest toolRoot(
      std::filesystem::absolute(std::filesystem::path(__FILE__))
          .parent_path()
          .parent_path()
          .parent_path());

  neuron::UserProfileSettings settings;
  settings.language = "tr";
  ASSERT_TRUE(neuron::saveUserProfileSettings(settings));

  ScopedStderrCapture capture;
  reportStringDiagnostics("parser", "demo.nr", ".",
                          {"demo.nr:1:1: error: Unexpected token: '.'"});

  ASSERT_TRUE(capture.stream.str().find("Beklenmeyen token: '.'") !=
              std::string::npos);
  ASSERT_TRUE(capture.stream.str().find("Raw:") == std::string::npos);
  ASSERT_TRUE(capture.stream.str().find("Unexpected token") == std::string::npos);
  std::filesystem::remove_all(root);
  return true;
}

TEST(DiagnosticEngineRendersSemanticTemplatesWithoutRawEnglishTail) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "neuron_diagnostic_engine_tests";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "Neuron");
  ScopedAppDataEnvVarForDiagnosticEngineTest env(root.string());
  ScopedToolRootForDiagnosticEngineTest toolRoot(
      std::filesystem::absolute(std::filesystem::path(__FILE__))
          .parent_path()
          .parent_path()
          .parent_path());

  neuron::UserProfileSettings settings;
  settings.language = "tr";
  ASSERT_TRUE(neuron::saveUserProfileSettings(settings));

  neuron::SemanticError error;
  error.message = "Variable is used before it is initialized: h";
  error.code = "N2204";
  error.arguments = {{"name", "h"}};
  error.location = {1, 1, "demo.nr"};

  ScopedStderrCapture capture;
  reportSemanticDiagnostics("demo.nr", "h.x();", {error});

  ASSERT_TRUE(capture.stream.str().find(
                  "Degisken baslatilmadan once kullaniliyor: h.") !=
              std::string::npos);
  ASSERT_TRUE(capture.stream.str().find("Raw:") == std::string::npos);
  ASSERT_TRUE(capture.stream.str().find("Variable is used before") ==
              std::string::npos);
  std::filesystem::remove_all(root);
  return true;
}

TEST(DiagnosticEngineRendersUnknownIdentifierTemplatesWithoutFallback) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "neuron_diagnostic_engine_tests";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "Neuron");
  ScopedAppDataEnvVarForDiagnosticEngineTest env(root.string());
  ScopedToolRootForDiagnosticEngineTest toolRoot(
      std::filesystem::absolute(std::filesystem::path(__FILE__))
          .parent_path()
          .parent_path()
          .parent_path());

  neuron::UserProfileSettings settings;
  settings.language = "tr";
  ASSERT_TRUE(neuron::saveUserProfileSettings(settings));

  neuron::SemanticError error;
  error.message = "Undefined identifier: f";
  error.code = "N2201";
  error.arguments = {{"name", "f"}};
  error.location = {1, 1, "demo.nr"};

  ScopedStderrCapture capture;
  reportSemanticDiagnostics("demo.nr", "f.x();", {error});

  ASSERT_TRUE(capture.stream.str().find(
                  "Basvurulan tanimlayici mevcut kapsamda tanimli degil: f.") !=
              std::string::npos);
  ASSERT_TRUE(capture.stream.str().find("NR2001") == std::string::npos);
  ASSERT_TRUE(capture.stream.str().find("Raw:") == std::string::npos);
  ASSERT_TRUE(capture.stream.str().find("Undefined identifier") ==
              std::string::npos);
  std::filesystem::remove_all(root);
  return true;
}

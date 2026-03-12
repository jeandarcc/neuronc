#include "main/UserProfileSettings.h"
#include "neuronc/cli/CommandDispatcher.h"
#include "neuronc/diagnostics/DiagnosticLocale.h"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace neuron;

namespace {

std::vector<char *> toLanguageArgv(std::vector<std::string> *args) {
  std::vector<char *> argv;
  argv.reserve(args->size());
  for (std::string &arg : *args) {
    argv.push_back(arg.data());
  }
  return argv;
}

neuron::cli::AppServices makeLanguageServices() {
  neuron::cli::AppServices services;
  services.parseFileArgWithTraceFlags =
      [](int, char *[], int, const std::string &) -> std::optional<std::string> {
    return std::nullopt;
  };
  services.printUsage = []() {};
  services.cmdRepl = []() { return 0; };
  services.isInteractiveInput = []() { return false; };
  services.cmdPackages = []() { return 0; };
  services.cmdNew = [](const std::string &, bool) { return 0; };
  services.cmdInstall = []() { return 0; };
  services.cmdAdd = [](const std::string &, const PackageInstallOptions &) {
    return 0;
  };
  services.cmdRemove = [](const std::string &, bool) { return 0; };
  services.cmdUpdate = [](const std::optional<std::string> &) { return 0; };
  services.cmdPublish = []() { return 0; };
  services.cmdSettingsOf = [](const std::string &) { return 0; };
  services.cmdDependenciesOf = [](const std::string &) { return 0; };
  services.cmdLex = [](const std::string &) { return 0; };
  services.cmdParse = [](const std::string &) { return 0; };
  services.cmdNir = [](const std::string &) { return 0; };
  services.cmdBuild = []() { return 0; };
  services.cmdBuildTarget = [](int, char *[]) { return 0; };
  services.cmdBuildMinimal = [](int, char *[]) { return 0; };
  services.cmdBuildProduct = [](int, char *[]) { return 0; };
  services.cmdCompile = [](const std::string &) { return 0; };
  services.cmdRun = []() { return 0; };
  services.cmdRunTarget = [](int, char *[]) { return 0; };
  services.cmdRelease = []() { return 0; };
  services.runNconCli = [](int, char *[], const char *) { return 0; };
  services.loadUserLanguage = []() {
    const auto settings = neuron::loadUserProfileSettings();
    return settings.has_value() ? settings->language : std::string("auto");
  };
  services.saveUserLanguage = [](const std::string &languageCode) {
    neuron::UserProfileSettings settings;
    settings.language = languageCode;
    return neuron::saveUserProfileSettings(settings);
  };
  services.resolveLanguage = [](const std::string &requestedLanguage,
                                const std::vector<std::string> &supportedLocales) {
    return neuron::diagnostics::resolveLanguagePreference(
        requestedLanguage, supportedLocales, std::string("tr-TR"));
  };
  return services;
}

struct ScopedEnvVar {
  explicit ScopedEnvVar(const std::string &value) {
#ifdef _WIN32
    _putenv_s("APPDATA", value.c_str());
#else
    (void)value;
#endif
  }
  ~ScopedEnvVar() {
#ifdef _WIN32
    _putenv_s("APPDATA", "");
#endif
  }
};

struct ScopedCoutCapture {
  std::ostringstream stream;
  std::streambuf *old = nullptr;

  ScopedCoutCapture() : old(std::cout.rdbuf(stream.rdbuf())) {}
  ~ScopedCoutCapture() { std::cout.rdbuf(old); }
};

} // namespace

TEST(CommandDispatcherShowsCurrentLanguageAndAllowsUpdates) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "npp_language_cli_tests";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  ScopedEnvVar env(root.string());

  neuron::cli::AppContext context;
  context.supportedDiagnosticLocales = {"en", "tr", "ja"};
  neuron::cli::AppServices services = makeLanguageServices();

  ScopedCoutCapture capture;

  std::vector<std::string> showArgs = {"neuron", "--language"};
  std::vector<char *> showArgv = toLanguageArgv(&showArgs);
  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(showArgv.size()),
                                         showArgv.data()),
            0);
  ASSERT_TRUE(capture.stream.str().find("Current diagnostic language: tr") !=
              std::string::npos);
  ASSERT_TRUE(capture.stream.str().find("Stored preference: auto") !=
              std::string::npos);

  capture.stream.str("");
  capture.stream.clear();
  std::vector<std::string> setArgs = {"neuron", "--language", "ja"};
  std::vector<char *> setArgv = toLanguageArgv(&setArgs);
  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(setArgv.size()),
                                         setArgv.data()),
            0);
  ASSERT_EQ(neuron::loadUserProfileSettings()->language, "ja");
  ASSERT_TRUE(capture.stream.str().find("Effective language: ja") !=
              std::string::npos);

  capture.stream.str("");
  capture.stream.clear();
  std::vector<std::string> autoArgs = {"neuron", "--language", "auto"};
  std::vector<char *> autoArgv = toLanguageArgv(&autoArgs);
  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(autoArgv.size()),
                                         autoArgv.data()),
            0);
  ASSERT_EQ(neuron::loadUserProfileSettings()->language, "auto");
  ASSERT_TRUE(capture.stream.str().find("Diagnostic language set to auto.") !=
              std::string::npos);

  std::filesystem::remove_all(root);
  return true;
}

TEST(DiagnosticLocaleFallsBackToEnglishForUnsupportedLanguage) {
  const auto resolved = neuron::diagnostics::resolveLanguagePreference(
      "pt-BR", {"en", "tr", "ja"}, std::string("tr-TR"));
  ASSERT_EQ(resolved.effective, "en");
  ASSERT_EQ(resolved.source,
            neuron::diagnostics::ResolvedLanguageSource::FallbackEnglish);
  return true;
}

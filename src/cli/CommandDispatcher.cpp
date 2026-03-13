#include "neuronc/cli/CommandDispatcher.h"

#include <cctype>
#include <iostream>
#include <optional>
#include <string>

namespace neuron::cli {

namespace {

std::string toLowerCopy(const std::string &value) {
  std::string lowered = value;
  for (char &ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return lowered;
}

std::optional<std::string> parseTargetArgument(int argc, char *argv[],
                                               int startIndex) {
  for (int i = startIndex; i < argc; ++i) {
    const std::string arg = argv[i];
    const std::string prefix = "--target=";
    if (arg.rfind(prefix, 0) == 0) {
      return arg.substr(prefix.size());
    }
    if (arg == "--target") {
      if ((i + 1) < argc) {
        return std::string(argv[i + 1]);
      }
      return std::string();
    }
  }
  return std::nullopt;
}

bool parseOptionValue(const std::string &arg, const std::string &flag,
                      std::string *outValue) {
  const std::string prefix = flag + "=";
  if (arg.rfind(prefix, 0) == 0) {
    if (outValue != nullptr) {
      *outValue = arg.substr(prefix.size());
    }
    return true;
  }
  return false;
}

std::optional<std::string> parseLanguageArgument(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--language") {
      if ((i + 1) < argc && argv[i + 1][0] != '-') {
        return std::string(argv[i + 1]);
      }
      return std::string();
    }

    std::string value;
    if (parseOptionValue(arg, "--language", &value)) {
      return value;
    }
  }
  return std::nullopt;
}

int handleLanguageOption(AppContext &context, const AppServices &services,
                         const std::optional<std::string> &languageArg) {
  if (!languageArg.has_value()) {
    return -1;
  }

  const std::string currentStored =
      services.loadUserLanguage ? services.loadUserLanguage() : "auto";

  if (languageArg->empty()) {
    context.diagnosticLanguage =
        services.resolveLanguage(currentStored, context.supportedDiagnosticLocales);
    std::cout << "Current diagnostic language: "
              << context.diagnosticLanguage.effective << " (source: "
              << neuron::diagnostics::resolvedLanguageSourceName(
                     context.diagnosticLanguage.source)
              << ")" << std::endl;
    std::cout << "Stored preference: " << currentStored << std::endl;
    std::cout << "Use 'neuron --language <LanguageCode>' to set a language, or "
                 "'neuron --language auto' to follow the OS language."
              << std::endl;
    return 0;
  }

  const std::string requested = *languageArg;
  const auto resolved =
      services.resolveLanguage(requested, context.supportedDiagnosticLocales);
  if (!services.saveUserLanguage || !services.saveUserLanguage(requested)) {
    std::cerr << "Failed to save language preference." << std::endl;
    return 1;
  }

  context.diagnosticLanguage = resolved;
  if (neuron::diagnostics::isLanguageAutoValue(requested)) {
    std::cout << "Diagnostic language set to auto. Effective language: "
              << resolved.effective << " (source: "
              << neuron::diagnostics::resolvedLanguageSourceName(resolved.source)
              << ")" << std::endl;
    return 0;
  }

  std::cout << "Diagnostic language set to '" << requested
            << "'. Effective language: " << resolved.effective;
  if (resolved.source ==
      neuron::diagnostics::ResolvedLanguageSource::FallbackEnglish) {
    std::cout << " (requested locale unavailable; using fallback en)";
  }
  std::cout << std::endl;
  return 0;
}

} // namespace

int dispatchCommand(AppContext &context, const AppServices &services, int argc,
                    char *argv[]) {
  if (argc < 2) {
    if (services.cmdRepl && services.isInteractiveInput &&
        services.isInteractiveInput()) {
      return services.cmdRepl();
    }
    services.printUsage();
    return 0;
  }

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--bypass-rules" || arg == "-bypass-rules") {
      context.bypassRules = true;
    }
  }
  if (context.bypassRules) {
    std::cout << "Bypassing project rules for this invocation (--bypass-rules)."
              << std::endl;
  }

  const std::optional<std::string> languageArg = parseLanguageArgument(argc, argv);
  const int languageResult = handleLanguageOption(context, services, languageArg);
  if (languageResult != -1) {
    return languageResult;
  }

  const std::string command = argv[1];

  if (command == "help" || command == "--help" || command == "-h") {
    services.printUsage();
    return 0;
  }

  if (command == "new" || command == "init") {
    if (argc < 3) {
      std::cerr << "Usage: neuron " << command << " <project-name>"
                << std::endl;
      return 1;
    }
    bool library = false;
    for (int i = 3; i < argc; ++i) {
      if (std::string(argv[i]) == "--lib") {
        library = true;
      }
    }
    return services.cmdNew(argv[2], library);
  }

  if (command == "packages") {
    return services.cmdPackages();
  }

  if (command == "install") {
    return services.cmdInstall();
  }

  if (command == "add") {
    if (argc < 3) {
      std::cerr << "Usage: neuron add <owner/repo|url> [--version X] [--tag T] "
                   "[--commit SHA] [--global|--local]"
                << std::endl;
      return 1;
    }
    neuron::PackageInstallOptions options;
    std::string packageSpec = argv[2];
    for (int i = 3; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--global") {
        options.global = true;
        continue;
      }
      if (arg == "--local") {
        options.global = false;
        continue;
      }
      if (arg == "--version" && (i + 1) < argc) {
        options.version = argv[++i];
        continue;
      }
      if (arg == "--tag" && (i + 1) < argc) {
        options.tag = argv[++i];
        continue;
      }
      if (arg == "--commit" && (i + 1) < argc) {
        options.commit = argv[++i];
        continue;
      }
      if (parseOptionValue(arg, "--version", &options.version) ||
          parseOptionValue(arg, "--tag", &options.tag) ||
          parseOptionValue(arg, "--commit", &options.commit)) {
        continue;
      }
      if (!arg.empty() && arg.front() != '-' && options.version.empty()) {
        options.version = arg;
        continue;
      }
      std::cerr << "Unknown add option: " << arg << std::endl;
      return 1;
    }
    return services.cmdAdd(packageSpec, options);
  }

  if (command == "remove") {
    if (argc < 3) {
      std::cerr << "Usage: neuron remove <package> [--global]" << std::endl;
      return 1;
    }
    bool removeGlobal = false;
    for (int i = 3; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--global") {
        removeGlobal = true;
        continue;
      }
      std::cerr << "Unknown remove option: " << arg << std::endl;
      return 1;
    }
    return services.cmdRemove(argv[2], removeGlobal);
  }

  if (command == "update") {
    if (argc >= 3) {
      return services.cmdUpdate(std::string(argv[2]));
    }
    return services.cmdUpdate(std::nullopt);
  }

  if (command == "publish") {
    return services.cmdPublish();
  }

  if (command == "settings-of") {
    if (argc < 3) {
      std::cerr << "Usage: neuron settings-of <builtin-module|package-name|owner/repo|url>"
                << std::endl;
      return 1;
    }
    return services.cmdSettingsOf(argv[2]);
  }

  if (command == "dependencies-of") {
    if (argc < 3) {
      std::cerr
          << "Usage: neuron dependencies-of <builtin-module|package-name|owner/repo|url>"
          << std::endl;
      return 1;
    }
    return services.cmdDependenciesOf(argv[2]);
  }

  if (command == "ncon") {
    return services.runNconCli(argc - 1, argv + 1,
                               argc > 0 ? argv[0] : nullptr);
  }

  if (command == "lex") {
    const auto fileArg = services.parseFileArgWithTraceFlags(
        argc, argv, 2,
        "Usage: neuron lex <file.nr> [--trace-errors] [--bypass-rules]");
    if (!fileArg.has_value()) {
      return 1;
    }
    return services.cmdLex(*fileArg);
  }

  if (command == "parse") {
    const auto fileArg = services.parseFileArgWithTraceFlags(
        argc, argv, 2,
        "Usage: neuron parse <file.nr> [--trace-errors] [--bypass-rules]");
    if (!fileArg.has_value()) {
      return 1;
    }
    return services.cmdParse(*fileArg);
  }

  if (command == "nir") {
    const auto fileArg = services.parseFileArgWithTraceFlags(
        argc, argv, 2,
        "Usage: neuron nir <file.nr> [--trace-errors] [--bypass-rules]");
    if (!fileArg.has_value()) {
      return 1;
    }
    return services.cmdNir(*fileArg);
  }

  if (command == "build") {
    const auto targetArg = parseTargetArgument(argc, argv, 2);
    if (targetArg.has_value()) {
      if (targetArg->empty()) {
        std::cerr << "Missing value for --target" << std::endl;
        return 1;
      }
      if (toLowerCopy(*targetArg) == "web") {
        return services.cmdBuildTarget(argc, argv);
      }
      std::cerr << "Unsupported build target: " << *targetArg << std::endl;
      return 1;
    }
    return services.cmdBuild();
  }

  if (command == "build-nucleus") {
    return services.cmdBuildMinimal(argc, argv);
  }

  if (command == "build-product") {
    return services.cmdBuildProduct(argc, argv);
  }

  if (command == "compile") {
    const auto fileArg = services.parseFileArgWithTraceFlags(
        argc, argv, 2,
        "Usage: neuron compile <file.nr> [--trace-errors] [--bypass-rules]");
    if (!fileArg.has_value()) {
      return 1;
    }
    return services.cmdCompile(*fileArg);
  }

  if (command == "run") {
    const auto targetArg = parseTargetArgument(argc, argv, 2);
    if (targetArg.has_value()) {
      if (targetArg->empty()) {
        std::cerr << "Missing value for --target" << std::endl;
        return 1;
      }
      if (toLowerCopy(*targetArg) == "web") {
        return services.cmdRunTarget(argc, argv);
      }
      std::cerr << "Unsupported run target: " << *targetArg << std::endl;
      return 1;
    }
    return services.cmdRun();
  }

  if (command == "surgeon") {
    return services.cmdSurgeon(argc, argv);
  }

  if (command == "release") {
    return services.cmdRelease();
  }

  std::cerr << "Unknown command: " << command << std::endl;
  services.printUsage();
  return 1;
}

} // namespace neuron::cli


// REPL and dispatcher tests - included from tests/test_main.cpp
#include "neuronc/cli/CommandDispatcher.h"
#include "neuronc/cli/repl/ReplPipeline.h"
#include "neuronc/codegen/JITEngine.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace neuron;

namespace {

struct ReplPipelineRuntimeState {
  int startupCalls = 0;
  int shutdownCalls = 0;
  int moduleInitCalls = 0;
  int inputIntCalls = 0;
  int inputStringCalls = 0;
  std::vector<std::int64_t> printedInts;
  std::vector<std::string> printedStrings;
  std::vector<std::int64_t> queuedInputInts;
  std::string queuedInputString;
};

ReplPipelineRuntimeState g_replPipelineRuntimeState;

void resetReplPipelineRuntimeState() {
  g_replPipelineRuntimeState.startupCalls = 0;
  g_replPipelineRuntimeState.shutdownCalls = 0;
  g_replPipelineRuntimeState.moduleInitCalls = 0;
  g_replPipelineRuntimeState.inputIntCalls = 0;
  g_replPipelineRuntimeState.inputStringCalls = 0;
  g_replPipelineRuntimeState.printedInts.clear();
  g_replPipelineRuntimeState.printedStrings.clear();
  g_replPipelineRuntimeState.queuedInputInts.clear();
  g_replPipelineRuntimeState.queuedInputString.clear();
}

void replPipelineTestRuntimeStartup() {
  ++g_replPipelineRuntimeState.startupCalls;
}

void replPipelineTestRuntimeShutdown() {
  ++g_replPipelineRuntimeState.shutdownCalls;
}

void replPipelineTestModuleInit(const char *) {
  ++g_replPipelineRuntimeState.moduleInitCalls;
}

void replPipelineTestPrintInt(std::int64_t value) {
  g_replPipelineRuntimeState.printedInts.push_back(value);
}

void replPipelineTestPrintStr(const char *value) {
  g_replPipelineRuntimeState.printedStrings.push_back(value == nullptr ? ""
                                                                       : value);
}

std::int64_t replPipelineTestInputInt(const char *, std::int64_t, std::int64_t,
                                      std::int64_t, std::int64_t, std::int64_t,
                                      std::int64_t, std::int64_t) {
  ++g_replPipelineRuntimeState.inputIntCalls;
  if (g_replPipelineRuntimeState.queuedInputInts.empty()) {
    return 0;
  }
  const std::int64_t value = g_replPipelineRuntimeState.queuedInputInts.front();
  g_replPipelineRuntimeState.queuedInputInts.erase(
      g_replPipelineRuntimeState.queuedInputInts.begin());
  return value;
}

const char *replPipelineTestInputString(const char *, std::int64_t, std::int64_t,
                                        const char *, std::int64_t) {
  ++g_replPipelineRuntimeState.inputStringCalls;
  return g_replPipelineRuntimeState.queuedInputString.c_str();
}

void replPipelineTestReplEchoString(const char *value) {
  std::string text = "'";
  if (value != nullptr) {
    text += value;
  }
  text += "'";
  g_replPipelineRuntimeState.printedStrings.push_back(std::move(text));
}

class ReplPipelineTestSymbolProvider : public neuron::JitSymbolProvider {
public:
  bool loadSymbols(std::unordered_map<std::string, void *> *outSymbols,
                   std::string *outError) override {
    if (outSymbols == nullptr) {
      if (outError != nullptr) {
        *outError = "missing symbol output";
      }
      return false;
    }

    outSymbols->clear();
    (*outSymbols)["neuron_runtime_startup"] =
        reinterpret_cast<void *>(&replPipelineTestRuntimeStartup);
    (*outSymbols)["neuron_runtime_shutdown"] =
        reinterpret_cast<void *>(&replPipelineTestRuntimeShutdown);
    (*outSymbols)["neuron_module_init"] =
        reinterpret_cast<void *>(&replPipelineTestModuleInit);
    (*outSymbols)["neuron_print_int"] =
        reinterpret_cast<void *>(&replPipelineTestPrintInt);
    (*outSymbols)["neuron_print_str"] =
        reinterpret_cast<void *>(&replPipelineTestPrintStr);
    (*outSymbols)["neuron_io_input_int"] =
        reinterpret_cast<void *>(&replPipelineTestInputInt);
    (*outSymbols)["neuron_io_input_string"] =
        reinterpret_cast<void *>(&replPipelineTestInputString);
    (*outSymbols)["neuron_repl_echo_string"] =
        reinterpret_cast<void *>(&replPipelineTestReplEchoString);
    return true;
  }
};

std::vector<char *> toArgvReplTests(std::vector<std::string> *args) {
  std::vector<char *> argv;
  argv.reserve(args->size());
  for (std::string &arg : *args) {
    argv.push_back(arg.data());
  }
  return argv;
}

neuron::cli::AppServices makeReplTestServices() {
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
  services.cmdAdd = [](const std::string &,
                       const PackageInstallOptions &) { return 0; };
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
  return services;
}

neuron::cli::ReplPipelineDependencies makeReplPipelineTestDeps() {
  neuron::cli::ReplPipelineDependencies deps;
  deps.loadNeuronSettings = [](const std::filesystem::path &) {
    neuron::cli::NeuronSettings settings;
    settings.agentHints = false;
    return settings;
  };
  deps.tryLoadProjectConfigFromCwd = []() {
    return std::optional<neuron::ProjectConfig>{};
  };
  deps.collectAvailableModules =
      [](const std::filesystem::path &,
         const std::optional<neuron::ProjectConfig> &) {
        return std::unordered_set<std::string>{};
      };
  deps.applyTensorRuntimeEnv = [](const std::optional<neuron::ProjectConfig> &) {};
  deps.toLLVMOptLevel = [](neuron::BuildOptimizeLevel) {
    return neuron::LLVMOptLevel::O0;
  };
  deps.toLLVMTargetCPU = [](neuron::BuildTargetCPU) {
    return neuron::LLVMTargetCPU::Generic;
  };
  deps.ensureRuntimeObjects = [](const neuron::LLVMCodeGenOptions &) {
    return true;
  };
  deps.runtimeSharedLibraryPath = []() { return std::filesystem::path(); };
  deps.initializeJitEngine = [](neuron::JITEngine *jit, std::string *outError) {
    return jit->initialize(std::make_unique<ReplPipelineTestSymbolProvider>(),
                           outError);
  };
  return deps;
}

} // namespace

TEST(CommandDispatcherStartsReplForInteractiveBareInvocation) {
  neuron::cli::AppContext context;
  neuron::cli::AppServices services = makeReplTestServices();
  bool replStarted = false;
  bool usagePrinted = false;

  services.isInteractiveInput = []() { return true; };
  services.cmdRepl = [&]() {
    replStarted = true;
    return 42;
  };
  services.printUsage = [&]() { usagePrinted = true; };

  std::vector<std::string> args = {"neuron"};
  std::vector<char *> argv = toArgvReplTests(&args);

  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(argv.size()),
                                         argv.data()),
            42);
  ASSERT_TRUE(replStarted);
  ASSERT_FALSE(usagePrinted);
  return true;
}

TEST(CommandDispatcherKeepsUsageForNonInteractiveBareInvocation) {
  neuron::cli::AppContext context;
  neuron::cli::AppServices services = makeReplTestServices();
  bool usagePrinted = false;
  bool replStarted = false;

  services.isInteractiveInput = []() { return false; };
  services.printUsage = [&]() { usagePrinted = true; };
  services.cmdRepl = [&]() {
    replStarted = true;
    return 1;
  };

  std::vector<std::string> args = {"neuron"};
  std::vector<char *> argv = toArgvReplTests(&args);

  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(argv.size()),
                                         argv.data()),
            0);
  ASSERT_TRUE(usagePrinted);
  ASSERT_FALSE(replStarted);
  return true;
}

TEST(CommandDispatcherHelpStillPrintsUsageBeforeRepl) {
  neuron::cli::AppContext context;
  neuron::cli::AppServices services = makeReplTestServices();
  bool usagePrinted = false;
  bool replStarted = false;

  services.isInteractiveInput = []() { return true; };
  services.printUsage = [&]() { usagePrinted = true; };
  services.cmdRepl = [&]() {
    replStarted = true;
    return 1;
  };

  std::vector<std::string> args = {"neuron", "help"};
  std::vector<char *> argv = toArgvReplTests(&args);

  ASSERT_EQ(neuron::cli::dispatchCommand(context, services,
                                         static_cast<int>(argv.size()),
                                         argv.data()),
            0);
  ASSERT_TRUE(usagePrinted);
  ASSERT_FALSE(replStarted);
  return true;
}

TEST(ReplPipelineReplaysCommittedCellsAndRejectsBrokenSubmit) {
  resetReplPipelineRuntimeState();

  neuron::cli::ReplSession session;
  neuron::cli::ReplPipeline pipeline(makeReplPipelineTestDeps());

  const neuron::cli::ReplSubmitResult first = pipeline.submit(&session, "x is 10;");
  ASSERT_TRUE(first.committed);
  ASSERT_EQ(session.cells().size(), static_cast<std::size_t>(1));
  ASSERT_TRUE(g_replPipelineRuntimeState.printedInts.empty());

  const neuron::cli::ReplSubmitResult second = pipeline.submit(&session, "Print(x);");
  ASSERT_TRUE(second.committed);
  ASSERT_EQ(session.cells().size(), static_cast<std::size_t>(2));
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts.size(),
            static_cast<std::size_t>(1));
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts[0], static_cast<std::int64_t>(10));

  const neuron::cli::ReplSubmitResult broken = pipeline.submit(&session, "Print(y);");
  ASSERT_FALSE(broken.committed);
  ASSERT_EQ(broken.phase, "semantic");
  ASSERT_EQ(session.cells().size(), static_cast<std::size_t>(2));
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts.size(),
            static_cast<std::size_t>(1));

  const neuron::cli::ReplSubmitResult third = pipeline.submit(&session, "Print(x);");
  ASSERT_TRUE(third.committed);
  ASSERT_EQ(session.cells().size(), static_cast<std::size_t>(3));
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts.size(),
            static_cast<std::size_t>(2));
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts[1], static_cast<std::int64_t>(10));
  ASSERT_EQ(g_replPipelineRuntimeState.startupCalls, 3);
  ASSERT_EQ(g_replPipelineRuntimeState.shutdownCalls, 3);
  ASSERT_EQ(g_replPipelineRuntimeState.moduleInitCalls, 3);
  return true;
}

TEST(ReplPipelineImplicitlyTerminatesSemicolonlessSubmits) {
  resetReplPipelineRuntimeState();

  neuron::cli::ReplSession session;
  neuron::cli::ReplPipeline pipeline(makeReplPipelineTestDeps());

  const neuron::cli::ReplSubmitResult first = pipeline.submit(&session, "x is 10");
  ASSERT_TRUE(first.committed);
  ASSERT_EQ(session.cells().size(), static_cast<std::size_t>(1));

  const neuron::cli::ReplSubmitResult second =
      pipeline.submit(&session, "Print(x)");
  ASSERT_TRUE(second.committed);
  ASSERT_EQ(session.cells().size(), static_cast<std::size_t>(2));
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts.size(),
            static_cast<std::size_t>(1));
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts[0], static_cast<std::int64_t>(10));
  return true;
}

TEST(ReplPipelineSuppressesCascadeAfterUnknownIdentifierMemberCall) {
  resetReplPipelineRuntimeState();

  neuron::cli::ReplSession session;
  neuron::cli::ReplPipeline pipeline(makeReplPipelineTestDeps());

  const neuron::cli::ReplSubmitResult result = pipeline.submit(&session, "f.x();");
  ASSERT_FALSE(result.committed);
  ASSERT_EQ(result.phase, "semantic");
  ASSERT_EQ(result.semanticDiagnostics.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(result.semanticDiagnostics[0].code, "N2201");
  ASSERT_EQ(result.semanticDiagnostics[0].message, "Undefined identifier: f");
  ASSERT_EQ(result.semanticDiagnostics[0].arguments.size(),
            static_cast<std::size_t>(1));
  const auto name = neuron::diagnostics::findDiagnosticArgument(
      result.semanticDiagnostics[0].arguments, "name");
  ASSERT_TRUE(name.has_value());
  ASSERT_EQ(name.value(), "f");
  ASSERT_TRUE(session.cells().empty());
  return true;
}

TEST(ReplPipelineReportsBareMethodNameWithCallGuidance) {
  resetReplPipelineRuntimeState();

  neuron::cli::ReplSession session;
  neuron::cli::ReplPipeline pipeline(makeReplPipelineTestDeps());

  const neuron::cli::ReplSubmitResult result = pipeline.submit(&session, "Print");
  ASSERT_FALSE(result.committed);
  ASSERT_EQ(result.phase, "semantic");
  ASSERT_EQ(result.semanticDiagnostics.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(
      result.semanticDiagnostics[0].message,
      "Identifier 'Print' refers to a method. Call it like 'Print(...)'; if "
      "you meant to declare a variable, start the name with a lowercase "
      "letter or '_'.");
  ASSERT_TRUE(session.cells().empty());
  return true;
}

TEST(ReplPipelineTreatsInputAsBuiltinMethodName) {
  resetReplPipelineRuntimeState();

  neuron::cli::ReplSession session;
  neuron::cli::ReplPipeline pipeline(makeReplPipelineTestDeps());

  const neuron::cli::ReplSubmitResult result = pipeline.submit(&session, "Input");
  ASSERT_FALSE(result.committed);
  ASSERT_EQ(result.phase, "semantic");
  ASSERT_EQ(result.semanticDiagnostics.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(
      result.semanticDiagnostics[0].message,
      "Identifier 'Input' refers to a method. Call it like 'Input(...)'; if "
      "you meant to declare a variable, start the name with a lowercase "
      "letter or '_'.");
  ASSERT_TRUE(session.cells().empty());
  return true;
}

TEST(ReplPipelineSuppressesBareInputReplayAndEchoesCurrentValue) {
  resetReplPipelineRuntimeState();
  g_replPipelineRuntimeState.queuedInputInts.push_back(41);

  neuron::cli::ReplSession session;
  neuron::cli::ReplPipeline pipeline(makeReplPipelineTestDeps());

  const neuron::cli::ReplSubmitResult first =
      pipeline.submit(&session, "Input<int>(\"Enter: \");");
  ASSERT_TRUE(first.committed);
  ASSERT_EQ(g_replPipelineRuntimeState.inputIntCalls, 1);
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts.size(),
            static_cast<std::size_t>(1));
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts[0], static_cast<std::int64_t>(41));

  const neuron::cli::ReplSubmitResult second = pipeline.submit(&session, "Print(7);");
  ASSERT_TRUE(second.committed);
  ASSERT_EQ(g_replPipelineRuntimeState.inputIntCalls, 1);
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts.size(),
            static_cast<std::size_t>(2));
  ASSERT_EQ(g_replPipelineRuntimeState.printedInts[1], static_cast<std::int64_t>(7));
  return true;
}

TEST(ReplPipelineDefaultsGenericlessInputToStringAndQuotesEcho) {
  resetReplPipelineRuntimeState();
  g_replPipelineRuntimeState.queuedInputString = "girilen";

  neuron::cli::ReplSession session;
  neuron::cli::ReplPipeline pipeline(makeReplPipelineTestDeps());

  const neuron::cli::ReplSubmitResult first =
      pipeline.submit(&session, "Input(\"Enter: \")");
  ASSERT_TRUE(first.committed);
  ASSERT_EQ(g_replPipelineRuntimeState.inputStringCalls, 1);
  ASSERT_EQ(g_replPipelineRuntimeState.printedStrings.size(),
            static_cast<std::size_t>(1));
  ASSERT_EQ(g_replPipelineRuntimeState.printedStrings[0], "'girilen'");

  const neuron::cli::ReplSubmitResult second =
      pipeline.submit(&session, "Print(\"ok\");");
  ASSERT_TRUE(second.committed);
  ASSERT_EQ(g_replPipelineRuntimeState.inputStringCalls, 1);
  ASSERT_EQ(g_replPipelineRuntimeState.printedStrings.size(),
            static_cast<std::size_t>(2));
  ASSERT_EQ(g_replPipelineRuntimeState.printedStrings[1], "ok");
  return true;
}

TEST(ReplPipelineContinuationHeuristicsCoverBlocksAndStrings) {
  ASSERT_TRUE(neuron::cli::ReplPipeline::needsContinuation("If(x) {"));
  ASSERT_TRUE(neuron::cli::ReplPipeline::needsContinuation("Print(\"abc"));
  ASSERT_TRUE(neuron::cli::ReplPipeline::needsContinuation("Foo("));
  ASSERT_TRUE(neuron::cli::ReplPipeline::needsContinuation("Print(x) \\"));
  ASSERT_FALSE(neuron::cli::ReplPipeline::needsContinuation("Print(x);"));
  ASSERT_FALSE(
      neuron::cli::ReplPipeline::needsContinuation("Print.... (\"Hello\");."));
  ASSERT_FALSE(neuron::cli::ReplPipeline::needsContinuation("System."));
  ASSERT_FALSE(neuron::cli::ReplPipeline::needsContinuation("x is 10;"));
  return true;
}

TEST(ReplPipelineSkipsRuntimeBuildWhenSharedLibraryAlreadyExists) {
  resetReplPipelineRuntimeState();

  const fs::path tempRuntimePath =
      fs::current_path() / "tmp_repl_prebuilt_runtime.dll";
  {
    std::ofstream out(tempRuntimePath, std::ios::trunc);
    out << "stub";
  }

  bool ensureCalled = false;
  neuron::cli::ReplPipelineDependencies deps = makeReplPipelineTestDeps();
  deps.runtimeSharedLibraryPath = [&]() { return tempRuntimePath; };
  deps.ensureRuntimeObjects = [&](const neuron::LLVMCodeGenOptions &) {
    ensureCalled = true;
    return true;
  };

  neuron::cli::ReplSession session;
  neuron::cli::ReplPipeline pipeline(std::move(deps));
  const neuron::cli::ReplSubmitResult result = pipeline.submit(&session, "x is 10;");

  std::error_code ec;
  fs::remove(tempRuntimePath, ec);

  ASSERT_TRUE(result.committed);
  ASSERT_FALSE(ensureCalled);
  return true;
}

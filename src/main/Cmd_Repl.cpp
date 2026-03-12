#include "CommandHandlers.h"

#include "BuildSupport.h"
#include "DiagnosticEngine.h"
#include "ProjectHelpers.h"
#include "SettingsLoader.h"

#include "neuronc/cli/repl/ReplConsoleReader.h"
#include "neuronc/cli/repl/ReplPipeline.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr const char *kNeuronReplVersion = "0.1.0";

std::string trimReplText(std::string text) {
  auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

const char *replPlatformLabel() {
#ifdef _WIN32
  return "win32";
#elif defined(__APPLE__)
  return "darwin";
#elif defined(__linux__)
  return "linux";
#else
  return "unknown";
#endif
}

neuron::cli::ReplPipelineDependencies makeReplDeps() {
  neuron::cli::ReplPipelineDependencies deps;
  deps.loadNeuronSettings = loadNeuronSettings;
  deps.tryLoadProjectConfigFromCwd = tryLoadProjectConfigFromCwd;
  deps.collectAvailableModules = collectAvailableModules;
  deps.applyTensorRuntimeEnv = applyTensorRuntimeEnv;
  deps.toLLVMOptLevel = toLLVMOptLevel;
  deps.toLLVMTargetCPU = toLLVMTargetCPU;
  deps.ensureRuntimeObjects = ensureRuntimeObjects;
  deps.runtimeSharedLibraryPath = jitRuntimeSharedLibraryPath;
  return deps;
}

void clearReplConsole() {
#ifdef _WIN32
  HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (outputHandle != INVALID_HANDLE_VALUE) {
    CONSOLE_SCREEN_BUFFER_INFO screenInfo{};
    if (GetConsoleScreenBufferInfo(outputHandle, &screenInfo)) {
      const DWORD cellCount = static_cast<DWORD>(
          screenInfo.dwSize.X * screenInfo.dwSize.Y);
      DWORD written = 0;
      const COORD home{0, 0};
      FillConsoleOutputCharacterA(outputHandle, ' ', cellCount, home, &written);
      FillConsoleOutputAttribute(outputHandle, screenInfo.wAttributes,
                                 cellCount, home, &written);
      SetConsoleCursorPosition(outputHandle, home);
      return;
    }
  }
#endif
  std::cout << "\x1b[2J\x1b[H";
  std::cout.flush();
}

void printReplHelpText() {
  std::cout
      << "REPL commands:\n"
      << "  :help   show this help\n"
      << "  :run    submit the current buffered input\n"
      << "  :reset  clear committed REPL state\n"
      << "  :clear  clear the console\n"
      << "  :quit   exit the REPL\n"
      << "\n"
      << "Input:\n"
      << "  Enter submits the current buffer.\n"
      << "  Incomplete syntax continues automatically.\n"
      << "  Ctrl+Enter inserts a newline without submit in the Windows console.\n";
}

std::string replSourceFor(const neuron::cli::ReplSubmitResult &result,
                          const std::string &file) {
  const auto it = result.sourceByFile.find(file);
  return it != result.sourceByFile.end() ? it->second : std::string();
}

void reportReplStringDiagnostics(const neuron::cli::ReplSubmitResult &result) {
  for (const std::string &diagnostic : result.stringDiagnostics) {
    const DiagnosticLocation location = parseDiagnosticLocation(diagnostic);
    const std::string file =
        (location.valid && !location.file.empty()) ? location.file : "<repl>";
    reportStringDiagnostics(result.phase, file, replSourceFor(result, file),
                            {diagnostic});
  }
}

void reportReplSemanticDiagnostics(const neuron::cli::ReplSubmitResult &result) {
  for (const neuron::SemanticError &diagnostic : result.semanticDiagnostics) {
    const std::string file =
        diagnostic.location.file.empty() ? "<repl>" : diagnostic.location.file;
    reportSemanticDiagnostics(file, replSourceFor(result, file), {diagnostic});
  }
}

void reportReplFailure(const neuron::cli::ReplSubmitResult &result) {
  if (!result.diagnostics.empty()) {
    reportFrontendDiagnostics(result.diagnostics, result.sourceByFile);
    return;
  }
  if (!result.stringDiagnostics.empty()) {
    reportReplStringDiagnostics(result);
    return;
  }
  if (!result.semanticDiagnostics.empty()) {
    reportReplSemanticDiagnostics(result);
    return;
  }
  if (!result.runtimeError.empty()) {
    printDiagnostic("<repl>", 1, 1, DiagnosticSeverity::Error,
                    diagnosticCodeForPhase(
                        result.phase.empty() ? "semantic" : result.phase,
                        DiagnosticSeverity::Error),
                    result.runtimeError);
  }
}

} // namespace

int cmdRepl() {
  neuron::cli::ReplPipeline pipeline(makeReplDeps());
  neuron::cli::ReplConsoleReader reader;
  neuron::cli::ReplSession session;
  std::string buffer;

  std::cout << "neuron++ " << kNeuronReplVersion << " (" << __DATE__ << ", "
            << __TIME__ << ") on " << replPlatformLabel() << std::endl;
  std::cout << "Type :help for more information." << std::endl;

  for (;;) {
    const std::string prompt = buffer.empty() ? ">>> " : "... ";
    neuron::cli::ReplReadResult input = reader.read(prompt);
    const std::string trimmed = trimReplText(input.text);
    bool forceSubmit = false;

    if (trimmed == ":quit") {
      break;
    }
    if (trimmed == ":help") {
      printReplHelpText();
      if (input.eof) {
        break;
      }
      continue;
    }
    if (trimmed == ":reset") {
      session.reset();
      buffer.clear();
      std::cout << "Session reset." << std::endl;
      if (input.eof) {
        break;
      }
      continue;
    }
    if (trimmed == ":clear") {
      clearReplConsole();
      if (input.eof) {
        break;
      }
      continue;
    }
    if (trimmed == ":run") {
      forceSubmit = true;
    } else if (!input.text.empty() || !buffer.empty()) {
      if (!buffer.empty()) {
        buffer.push_back('\n');
      }
      buffer += input.text;
    }

    if (input.eof && buffer.empty()) {
      break;
    }
    if (buffer.empty()) {
      if (forceSubmit) {
        std::cout << "Buffer is empty." << std::endl;
      }
      if (input.eof) {
        break;
      }
      continue;
    }

    if (!forceSubmit && !input.eof &&
        neuron::cli::ReplPipeline::needsContinuation(buffer)) {
      continue;
    }

    reader.remember(buffer);
    const neuron::cli::ReplSubmitResult result = pipeline.submit(&session, buffer);
    if (!result.committed) {
      reportReplFailure(result);
    }
    buffer.clear();

    if (input.eof) {
      break;
    }
  }

  return 0;
}

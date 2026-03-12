// ORC JIT engine tests - included from tests/test_main.cpp
#include "neuronc/codegen/JITEngine.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace neuron;
namespace fs = std::filesystem;

namespace {

struct JitEngineRuntimeState {
  int startupCalls = 0;
  int shutdownCalls = 0;
  int moduleInitCalls = 0;
};

JitEngineRuntimeState g_jitEngineRuntimeState;

void resetJitEngineRuntimeState() {
  g_jitEngineRuntimeState.startupCalls = 0;
  g_jitEngineRuntimeState.shutdownCalls = 0;
  g_jitEngineRuntimeState.moduleInitCalls = 0;
}

void jitEngineTestRuntimeStartup() {
  ++g_jitEngineRuntimeState.startupCalls;
}

void jitEngineTestRuntimeShutdown() {
  ++g_jitEngineRuntimeState.shutdownCalls;
}

void jitEngineTestModuleInit(const char *) {
  ++g_jitEngineRuntimeState.moduleInitCalls;
}

class JitEngineTestSymbolProvider : public neuron::JitSymbolProvider {
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
        reinterpret_cast<void *>(&jitEngineTestRuntimeStartup);
    (*outSymbols)["neuron_runtime_shutdown"] =
        reinterpret_cast<void *>(&jitEngineTestRuntimeShutdown);
    (*outSymbols)["neuron_module_init"] =
        reinterpret_cast<void *>(&jitEngineTestModuleInit);
    return true;
  }
};

fs::path currentExecutablePathForTests() {
#ifdef _WIN32
  std::vector<wchar_t> buffer(MAX_PATH, L'\0');
  for (;;) {
    const DWORD written = GetModuleFileNameW(nullptr, buffer.data(),
                                             static_cast<DWORD>(buffer.size()));
    if (written == 0) {
      return {};
    }
    if (written < buffer.size() - 1) {
      return fs::path(std::wstring(buffer.data(), written));
    }
    buffer.resize(buffer.size() * 2, L'\0');
  }
#else
  std::vector<char> buffer(4096, '\0');
  for (;;) {
    const ssize_t written =
        readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (written < 0) {
      return {};
    }
    if (static_cast<std::size_t>(written) < buffer.size() - 1) {
      buffer[static_cast<std::size_t>(written)] = '\0';
      return fs::path(buffer.data());
    }
    buffer.resize(buffer.size() * 2, '\0');
  }
#endif
}

fs::path runtimeLibraryPathForTests() {
  const fs::path exeDir = currentExecutablePathForTests().parent_path();
#ifdef _WIN32
  return exeDir / "libneuron_runtime.dll";
#elif defined(__APPLE__)
  return exeDir / "libneuron_runtime.dylib";
#else
  return exeDir / "libneuron_runtime.so";
#endif
}

} // namespace

TEST(JITEngineExecutesGeneratedMainWithStubRuntime) {
  resetJitEngineRuntimeState();

  auto module = std::make_unique<nir::Module>("jit_engine_smoke");
  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");
  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  ASSERT_TRUE(codegen.verifyModuleIR(&verifyError));

  neuron::JITEngine jit;
  std::string initError;
  ASSERT_TRUE(jit.initialize(std::make_unique<JitEngineTestSymbolProvider>(),
                             &initError));

  std::string addError;
  ASSERT_TRUE(jit.addModule(codegen.takeOwnedModule(), &addError));

  int exitCode = -1;
  std::string runError;
  ASSERT_TRUE(jit.executeMain(&exitCode, &runError));
  ASSERT_EQ(exitCode, 0);
  ASSERT_EQ(g_jitEngineRuntimeState.startupCalls, 1);
  ASSERT_EQ(g_jitEngineRuntimeState.shutdownCalls, 1);
  ASSERT_EQ(g_jitEngineRuntimeState.moduleInitCalls, 1);
  return true;
}

TEST(SharedLibraryJitSymbolProviderLoadsBuiltRuntimeLibrary) {
  const fs::path runtimeLibrary = runtimeLibraryPathForTests();
  ASSERT_TRUE(!runtimeLibrary.empty());
  ASSERT_TRUE(fs::exists(runtimeLibrary));

  neuron::SharedLibraryJitSymbolProvider provider(runtimeLibrary);
  std::unordered_map<std::string, void *> symbols;
  std::string error;
  ASSERT_TRUE(provider.loadSymbols(&symbols, &error));
  ASSERT_TRUE(symbols.find("neuron_print_int") != symbols.end());
  ASSERT_TRUE(symbols.find("neuron_print_str") != symbols.end());
  return true;
}

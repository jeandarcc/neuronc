#pragma once

#include "neuronc/codegen/LLVMCodeGen.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace llvm {
namespace orc {
class LLJIT;
}
namespace sys {
class DynamicLibrary;
}
} // namespace llvm

namespace neuron {

class JitSymbolProvider {
public:
  virtual ~JitSymbolProvider() = default;

  virtual bool loadSymbols(std::unordered_map<std::string, void *> *outSymbols,
                           std::string *outError) = 0;
};

class CurrentProcessJitSymbolProvider : public JitSymbolProvider {
public:
  bool loadSymbols(std::unordered_map<std::string, void *> *outSymbols,
                   std::string *outError) override;
};

class SharedLibraryJitSymbolProvider : public JitSymbolProvider {
public:
  explicit SharedLibraryJitSymbolProvider(std::filesystem::path libraryPath);

  bool loadSymbols(std::unordered_map<std::string, void *> *outSymbols,
                   std::string *outError) override;

private:
  std::filesystem::path m_libraryPath;
};

class JITEngine {
public:
  JITEngine();
  ~JITEngine();

  bool initialize(std::unique_ptr<JitSymbolProvider> symbolProvider,
                  std::string *outError = nullptr);
  bool addModule(OwnedLLVMModule module, std::string *outError = nullptr);
  bool executeMain(int *outExitCode = nullptr, std::string *outError = nullptr);

private:
  std::unique_ptr<JitSymbolProvider> m_symbolProvider;
  std::unique_ptr<llvm::orc::LLJIT> m_jit;
};

} // namespace neuron

#include "neuronc/codegen/llvm/LLVMObjectEmitter.h"

#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/IR/LegacyPassManager.h>

#include <memory>
#include <optional>

namespace neuron {
namespace codegen::llvm_support {
namespace {

bool isWasmTargetTriple(const std::string &triple) {
  if (triple.empty()) {
    return false;
  }
  const llvm::Triple parsed(triple);
  return parsed.isWasm();
}

void initializeTargetsForTriple(const std::string &triple) {
  if (isWasmTargetTriple(triple)) {
    LLVMInitializeWebAssemblyTargetInfo();
    LLVMInitializeWebAssemblyTarget();
    LLVMInitializeWebAssemblyTargetMC();
    LLVMInitializeWebAssemblyAsmPrinter();
    LLVMInitializeWebAssemblyAsmParser();
    return;
  }

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
}

llvm::CodeGenOptLevel mapCodeGenOptLevel(LLVMOptLevel level) {
  switch (level) {
  case LLVMOptLevel::O0:
    return llvm::CodeGenOptLevel::None;
  case LLVMOptLevel::O1:
    return llvm::CodeGenOptLevel::Less;
  case LLVMOptLevel::O2:
    return llvm::CodeGenOptLevel::Default;
  case LLVMOptLevel::O3:
  case LLVMOptLevel::Aggressive:
    return llvm::CodeGenOptLevel::Aggressive;
  case LLVMOptLevel::Oz:
    return llvm::CodeGenOptLevel::Default;
  }
  return llvm::CodeGenOptLevel::Aggressive;
}

std::string buildFeatureString() {
  const llvm::StringMap<bool> features = llvm::sys::getHostCPUFeatures();
  std::string out;
  for (const auto &feature : features) {
    if (!out.empty()) {
      out.push_back(',');
    }
    out.push_back(feature.second ? '+' : '-');
    out += feature.first().str();
  }
  return out;
}

std::unique_ptr<llvm::TargetMachine>
createTargetMachine(llvm::Module *module, const LLVMCodeGenOptions &options,
                    std::string *outError) {
  const std::string targetTripleStr =
      options.targetTripleOverride.empty()
          ? llvm::sys::getDefaultTargetTriple()
          : options.targetTripleOverride;
  llvm::Triple targetTriple(targetTripleStr);
  module->setTargetTriple(targetTriple);

  std::string error;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(targetTriple, error);
  if (target == nullptr) {
    if (outError != nullptr) {
      *outError = "Target lookup error: " + error;
    }
    return nullptr;
  }

  std::string cpu = "generic";
  std::string features;
  if (isWasmTargetTriple(targetTripleStr)) {
    if (options.enableWasmSimd) {
      features = "+simd128";
    }
  } else if (options.targetCPU == LLVMTargetCPU::Native) {
    cpu = llvm::sys::getHostCPUName().str();
    if (cpu.empty()) {
      cpu = "generic";
    }
    features = buildFeatureString();
  }

  llvm::TargetOptions targetOptions;
  if (options.optLevel == LLVMOptLevel::Aggressive) {
    targetOptions.AllowFPOpFusion = llvm::FPOpFusion::Fast;
    targetOptions.UnsafeFPMath = true;
    targetOptions.NoInfsFPMath = true;
    targetOptions.NoNaNsFPMath = true;
    targetOptions.NoTrappingFPMath = true;
  }

  auto targetMachine = std::unique_ptr<llvm::TargetMachine>(
      target->createTargetMachine(targetTriple, cpu, features, targetOptions,
                                  llvm::Reloc::PIC_, std::nullopt,
                                  mapCodeGenOptLevel(options.optLevel)));
  if (!targetMachine) {
    if (outError != nullptr) {
      *outError = "Failed to create target machine.";
    }
    return nullptr;
  }

  module->setDataLayout(targetMachine->createDataLayout());
  return targetMachine;
}

} // namespace

bool compileModuleToObject(llvm::Module *module, const std::string &filename,
                           const LLVMCodeGenOptions &options,
                           std::string *outError) {
  const std::string targetTripleStr =
      options.targetTripleOverride.empty()
          ? llvm::sys::getDefaultTargetTriple()
          : options.targetTripleOverride;
  initializeTargetsForTriple(targetTripleStr);

  auto targetMachine = createTargetMachine(module, options, outError);
  if (!targetMachine) {
    return false;
  }

  std::error_code ec;
  llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
  if (ec) {
    if (outError != nullptr) {
      *outError = "Could not open object file: " + ec.message();
    }
    return false;
  }

  llvm::legacy::PassManager pass;
  if (targetMachine->addPassesToEmitFile(pass, dest, nullptr,
                                         llvm::CodeGenFileType::ObjectFile)) {
    if (outError != nullptr) {
      *outError = "Target machine can't emit object file.";
    }
    return false;
  }

  pass.run(*module);
  dest.flush();
  return true;
}

} // namespace codegen::llvm_support
} // namespace neuron


#include "neuronc/codegen/LLVMCodeGen.h"
#include "neuronc/codegen/llvm/LLVMTypeLowering.h"
#include "neuronc/codegen/llvm/LLVMObjectEmitter.h"
#include "neuronc/cli/WebShaderTranspiler.h"
#include "neuronc/fusion/FusionBuiltins.h"

#include <llvm/ADT/StringMap.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/ADCE.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include "neuronc/codegen/llvm/LLVMRuntimeRegistry.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace neuron {

namespace {
namespace fs = std::filesystem;

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

llvm::OptimizationLevel mapOptLevel(LLVMOptLevel level) {
  switch (level) {
  case LLVMOptLevel::O0:
    return llvm::OptimizationLevel::O0;
  case LLVMOptLevel::O1:
    return llvm::OptimizationLevel::O1;
  case LLVMOptLevel::O2:
    return llvm::OptimizationLevel::O2;
  case LLVMOptLevel::O3:
  case LLVMOptLevel::Aggressive:
    return llvm::OptimizationLevel::O3;
  case LLVMOptLevel::Oz:
    return llvm::OptimizationLevel::Oz;
  }
  return llvm::OptimizationLevel::O3;
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

llvm::Constant *defaultValueForType(llvm::Type *type) {
  if (type == nullptr || type->isVoidTy()) {
    return nullptr;
  }
  return llvm::Constant::getNullValue(type);
}

std::string makeThreadThunkName(const std::string &targetName) {
  std::string out = "__neuron_thread_thunk_";
  out.reserve(out.size() + targetName.size());
  for (char ch : targetName) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (std::isalnum(c) || ch == '_') {
      out.push_back(ch);
    } else {
      out.push_back('_');
    }
  }
  return out;
}

std::string joinParameterTypes(const std::vector<std::string> &parameterTypes) {
  std::ostringstream out;
  for (size_t i = 0; i < parameterTypes.size(); ++i) {
    if (i != 0) {
      out << ',';
    }
    out << parameterTypes[i];
  }
  return out.str();
}

int32_t executionHintToRuntimeValue(nir::ExecutionHint hint) {
  return hint == nir::ExecutionHint::GpuPrefer ? 1 : 0;
}

std::string quoteShellArg(const std::string &value) {
  std::string quoted = "\"";
  for (char ch : value) {
    if (ch == '"') {
      quoted += "\\\"";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted += "\"";
  return quoted;
}

std::string shaderStageExtension(bool isVertexStage) {
  return isVertexStage ? ".vert" : ".frag";
}

std::string shaderStageLabel(bool isVertexStage) {
  return isVertexStage ? "vert" : "frag";
}

std::string shaderStageOption(bool isVertexStage) {
  return isVertexStage ? "vert" : "frag";
}

std::string hashShaderSource(const std::string &source) {
  return std::to_string(std::hash<std::string>{}(source));
}

std::vector<uint32_t> readSpirvWords(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    throw std::runtime_error("failed to open SPIR-V file: " + path.string());
  }
  std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
  if (bytes.empty() || (bytes.size() % 4u) != 0u) {
    throw std::runtime_error("invalid SPIR-V byte size: " + path.string());
  }
  std::vector<uint32_t> words(bytes.size() / 4u, 0u);
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

std::string shaderCompilerCommand(const fs::path &inputPath,
                                  const fs::path &outputPath,
                                  bool isVertexStage) {
  std::vector<std::string> tools = {"glslangValidator", "glslc"};
  for (const auto &tool : tools) {
    if (tool == "glslangValidator") {
      std::string command = quoteShellArg(tool) + " -V -S " +
                            shaderStageOption(isVertexStage) + " " +
                            quoteShellArg(inputPath.string()) + " -o " +
                            quoteShellArg(outputPath.string()) + " >nul 2>&1";
      if (std::system((quoteShellArg(tool) + " --version >nul 2>&1").c_str()) ==
          0) {
        return command;
      }
      continue;
    }
    std::string command = quoteShellArg(tool) + " -fshader-stage=" +
                          shaderStageOption(isVertexStage) + " " +
                          quoteShellArg(inputPath.string()) + " -o " +
                          quoteShellArg(outputPath.string()) + " >nul 2>&1";
    if (std::system((quoteShellArg(tool) + " --version >nul 2>&1").c_str()) ==
        0) {
      return command;
    }
  }
  throw std::runtime_error(
      "no GLSL shader compiler found; expected glslangValidator or glslc");
}

std::vector<uint32_t> compileShaderStageToSpirv(const std::string &glslSource,
                                                bool isVertexStage) {
  const fs::path cacheDir = fs::temp_directory_path() / "npp_shader_cache";
  std::error_code ec;
  fs::create_directories(cacheDir, ec);

  const std::string key =
      hashShaderSource((isVertexStage ? "vert:" : "frag:") + glslSource);
  const fs::path sourcePath = cacheDir / (key + shaderStageExtension(isVertexStage));
  const fs::path outputPath = cacheDir / (key + ".spv");

  if (!fs::exists(outputPath)) {
    std::ofstream out(sourcePath, std::ios::binary);
    if (!out.is_open()) {
      throw std::runtime_error("failed to write shader source: " +
                               sourcePath.string());
    }
    out << glslSource;
    out.close();

    const std::string command =
        shaderCompilerCommand(sourcePath, outputPath, isVertexStage);
    if (std::system(command.c_str()) != 0 || !fs::exists(outputPath)) {
      throw std::runtime_error("shader compilation failed for: " +
                               sourcePath.string());
    }
  }

  return readSpirvWords(outputPath);
}

fs::path shaderSpirvOutputPathForSource(const std::string &glslSource,
                                        bool isVertexStage) {
  const fs::path cacheDir = fs::temp_directory_path() / "npp_shader_cache";
  const std::string key =
      hashShaderSource((isVertexStage ? "vert:" : "frag:") + glslSource);
  return cacheDir / (key + ".spv");
}

std::string sanitizeShaderArtifactName(const std::string &name) {
  std::string sanitized;
  sanitized.reserve(name.size());
  for (char ch : name) {
    if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') {
      sanitized.push_back(ch);
    } else {
      sanitized.push_back('_');
    }
  }
  if (sanitized.empty()) {
    sanitized = "shader";
  }
  return sanitized;
}

bool writeBinaryFile(const fs::path &path, const std::vector<uint32_t> &words,
                     std::string *outError = nullptr) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create shader artifact directory: " + ec.message();
    }
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open shader artifact file for write: " +
                  path.string();
    }
    return false;
  }
  out.write(reinterpret_cast<const char *>(words.data()),
            static_cast<std::streamsize>(words.size() * sizeof(uint32_t)));
  return out.good();
}

bool writeTextFile(const fs::path &path, const std::string &text,
                   std::string *outError = nullptr) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create shader artifact directory: " + ec.message();
    }
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open shader artifact file for write: " +
                  path.string();
    }
    return false;
  }
  out << text;
  return out.good();
}

llvm::Constant *createGlobalCStringPtr(llvm::IRBuilder<> &builder,
                                       llvm::Module &module,
                                       llvm::LLVMContext &context,
                                       llvm::StringRef value,
                                       const llvm::Twine &name = "") {
  llvm::GlobalVariable *global =
      builder.CreateGlobalString(value, name, 0, &module);
  llvm::Constant *zero =
      llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
  llvm::Constant *indices[] = {zero, zero};
  return llvm::ConstantExpr::getInBoundsGetElementPtr(global->getValueType(),
                                                      global, indices);
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
  auto targetMachine =
      std::unique_ptr<llvm::TargetMachine>(target->createTargetMachine(
          targetTriple, cpu, features, targetOptions, llvm::Reloc::PIC_,
          std::nullopt, mapCodeGenOptLevel(options.optLevel)));
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

LLVMCodeGen::LLVMCodeGen() {
  m_context = std::make_unique<llvm::LLVMContext>();
  m_module = std::make_unique<llvm::Module>("neuron_module", *m_context);
  m_builder = std::make_unique<llvm::IRBuilder<>>(*m_context);
}

void LLVMCodeGen::setModuleCppExports(
    const std::unordered_map<std::string, ModuleCppCompileExport> &exports) {
  m_moduleCppExports = exports;
}

void LLVMCodeGen::setEntryFunctionName(std::string entryFunctionName) {
  m_entryFunctionName = std::move(entryFunctionName);
}

void LLVMCodeGen::setGraphicsShaderOutputDirectory(
    std::filesystem::path outputDirectory) {
  m_graphicsShaderOutputDirectory = std::move(outputDirectory);
}

void LLVMCodeGen::setGraphicsShaderCacheDirectory(
    std::filesystem::path cacheDirectory) {
  m_graphicsShaderCacheDirectory = std::move(cacheDirectory);
}

void LLVMCodeGen::setGraphicsShaderAllowCache(bool allowCache) {
  m_graphicsShaderAllowCache = allowCache;
}

llvm::Type *LLVMCodeGen::toLLVMType(NTypePtr type) {
  codegen::llvm_support::LLVMTypeLoweringState state;
  state.context = m_context.get();
  state.structMap = &m_structMap;
  return codegen::llvm_support::toLLVMType(state, type);
}

llvm::Value *LLVMCodeGen::toLLVMValue(nir::Value *nirVal) {
  if (!nirVal)
    return nullptr;

  // Check if already mapped
  auto it = m_valueMap.find(nirVal);
  if (it != m_valueMap.end()) {
    return it->second;
  }

  // Constants
  if (nirVal->getValueKind() == nir::ValueKind::ConstantInt) {
    auto *ci = static_cast<nir::ConstantInt *>(nirVal);
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*m_context),
                                  ci->getValue());
  }
  if (nirVal->getValueKind() == nir::ValueKind::ConstantFloat) {
    auto *cf = static_cast<nir::ConstantFloat *>(nirVal);
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*m_context),
                                 cf->getValue());
  }
  if (nirVal->getValueKind() == nir::ValueKind::ConstantString) {
    auto *cs = static_cast<nir::ConstantString *>(nirVal);
    return createGlobalCStringPtr(*m_builder, *m_module, *m_context,
                                  llvm::StringRef(cs->getValue()));
  }

  std::cerr << "LLVMCodeGen: toLLVMValue failed to map value '"
            << nirVal->getName() << "' (TypeID/Kind unknown?)" << std::endl;
  return nullptr;
}

llvm::Constant *
LLVMCodeGen::toGraphicsShaderDescriptor(std::string_view shaderName) {
  if (shaderName.empty() || m_sourceModule == nullptr) {
    return nullptr;
  }

  const auto cacheIt =
      m_graphicsShaderDescriptorMap.find(std::string(shaderName));
  if (cacheIt != m_graphicsShaderDescriptorMap.end()) {
    return cacheIt->second;
  }

  const nir::ShaderDesc *shader =
      m_sourceModule->findShader(std::string(shaderName));
  if (shader == nullptr) {
    return nullptr;
  }

  auto *i32Type = llvm::Type::getInt32Ty(*m_context);
  auto *ptrType = llvm::PointerType::get(*m_context, 0);
  auto *bindingStructType =
      llvm::StructType::get(*m_context,
                            {ptrType, i32Type, i32Type, i32Type, i32Type,
                             i32Type});
  auto *u32PtrType = llvm::PointerType::get(i32Type, 0);
  auto *descriptorStructType = llvm::StructType::get(
      *m_context,
      {ptrType, i32Type, i32Type, ptrType, i32Type, i32Type, i32Type, u32PtrType,
       i32Type, u32PtrType, i32Type, ptrType, i32Type, ptrType, i32Type});

  std::vector<llvm::Constant *> bindingConstants;
  bindingConstants.reserve(shader->bindings.size());
  for (const auto &binding : shader->bindings) {
    llvm::Constant *bindingName = createGlobalCStringPtr(
        *m_builder, *m_module, *m_context, llvm::StringRef(binding.name),
        "shader_binding_name");
    bindingConstants.push_back(llvm::ConstantStruct::get(
        bindingStructType,
        {bindingName,
         llvm::ConstantInt::get(i32Type, static_cast<uint32_t>(binding.kind)),
         llvm::ConstantInt::get(i32Type, binding.slot),
         llvm::ConstantInt::get(i32Type, binding.descriptorBinding),
         llvm::ConstantInt::get(i32Type, binding.uniformOffset),
         llvm::ConstantInt::get(i32Type, binding.uniformSize)}));
  }

  llvm::Constant *bindingArrayPtr = llvm::ConstantPointerNull::get(ptrType);
  if (!bindingConstants.empty()) {
    auto *bindingArrayType =
        llvm::ArrayType::get(bindingStructType, bindingConstants.size());
    auto *bindingArrayInit =
        llvm::ConstantArray::get(bindingArrayType, bindingConstants);
    auto *bindingArrayGlobal = new llvm::GlobalVariable(
        *m_module, bindingArrayType, true, llvm::GlobalValue::PrivateLinkage,
        bindingArrayInit, "neuron_shader_bindings");
    bindingArrayGlobal->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    bindingArrayGlobal->setAlignment(llvm::Align(8));

    llvm::Constant *zero =
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), 0);
    llvm::Constant *indices[] = {zero, zero};
    bindingArrayPtr = llvm::ConstantExpr::getInBoundsGetElementPtr(
        bindingArrayType, bindingArrayGlobal, indices);
  }

  uint32_t stageMask = 0;
  if (shader->hasVertexStage) {
    stageMask |= 1u;
  }
  if (shader->hasFragmentStage) {
    stageMask |= 2u;
  }

  auto createSpirvArray = [&](const std::string &glslSource,
                              bool isVertexStage) -> llvm::Constant * {
    if (glslSource.empty()) {
      return llvm::ConstantPointerNull::get(u32PtrType);
    }
    const std::string cacheKey =
        (isVertexStage ? "vert:" : "frag:") + glslSource;
    auto cacheIt = m_graphicsSpirvCache.find(cacheKey);
    if (cacheIt == m_graphicsSpirvCache.end()) {
      cacheIt = m_graphicsSpirvCache
                    .emplace(cacheKey,
                             compileShaderStageToSpirv(glslSource, isVertexStage))
                    .first;
    }
    auto *spirvArrayType =
        llvm::ArrayType::get(i32Type, cacheIt->second.size());
    std::vector<llvm::Constant *> words;
    words.reserve(cacheIt->second.size());
    for (uint32_t word : cacheIt->second) {
      words.push_back(llvm::ConstantInt::get(i32Type, word));
    }
    auto *spirvArrayGlobal = new llvm::GlobalVariable(
        *m_module, spirvArrayType, true, llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(spirvArrayType, words), "neuron_shader_spirv");
    spirvArrayGlobal->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    spirvArrayGlobal->setAlignment(llvm::Align(4));
    llvm::Constant *zero =
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), 0);
    llvm::Constant *indices[] = {zero, zero};
    return llvm::ConstantExpr::getInBoundsGetElementPtr(spirvArrayType,
                                                        spirvArrayGlobal,
                                                        indices);
  };

  llvm::Constant *vertexSpirvPtr =
      createSpirvArray(shader->vertexGlsl, true);
  llvm::Constant *fragmentSpirvPtr =
      createSpirvArray(shader->fragmentGlsl, false);
  const uint32_t vertexWordCount =
      shader->vertexGlsl.empty()
          ? 0u
          : static_cast<uint32_t>(
                m_graphicsSpirvCache.at("vert:" + shader->vertexGlsl).size());
  const uint32_t fragmentWordCount =
      shader->fragmentGlsl.empty()
          ? 0u
          : static_cast<uint32_t>(
                m_graphicsSpirvCache.at("frag:" + shader->fragmentGlsl).size());

  llvm::Constant *vertexWgslPtr = llvm::ConstantPointerNull::get(ptrType);
  llvm::Constant *fragmentWgslPtr = llvm::ConstantPointerNull::get(ptrType);
  uint32_t vertexWgslSize = 0;
  uint32_t fragmentWgslSize = 0;
  if (!m_graphicsShaderOutputDirectory.empty()) {
    std::error_code ec;
    fs::create_directories(m_graphicsShaderOutputDirectory, ec);
  }
  if (!m_graphicsShaderCacheDirectory.empty()) {
    std::error_code ec;
    fs::create_directories(m_graphicsShaderCacheDirectory, ec);
  }

  auto transpileWgsl = [&](const std::string &glslSource,
                           bool isVertexStage) -> std::string {
    if (glslSource.empty()) {
      return std::string();
    }
    const std::string cacheKey =
        shaderStageLabel(isVertexStage) + std::string(":") + glslSource;
    auto cacheIt = m_graphicsWgslCache.find(cacheKey);
    if (cacheIt != m_graphicsWgslCache.end()) {
      return cacheIt->second;
    }

    (void)compileShaderStageToSpirv(glslSource, isVertexStage);
    cli::WebShaderTranspileOptions transpileOptions;
    transpileOptions.allowCache = m_graphicsShaderAllowCache;
    transpileOptions.cacheDirectory =
        m_graphicsShaderCacheDirectory.empty()
            ? fs::temp_directory_path() / "npp_shader_cache" / "wgsl"
            : m_graphicsShaderCacheDirectory;
    cli::WebShaderTranspileResult transpileResult;
    const fs::path spirvPath =
        shaderSpirvOutputPathForSource(glslSource, isVertexStage);
    if (!cli::transpileSpirvToWgsl(spirvPath, transpileOptions,
                                   &transpileResult)) {
      throw std::runtime_error("WGSL shader transpile failed for: " +
                               spirvPath.string() + ": " +
                               transpileResult.error);
    }
    return m_graphicsWgslCache
        .emplace(cacheKey, transpileResult.outputText)
        .first->second;
  };

  const std::string vertexWgsl = !m_graphicsShaderOutputDirectory.empty()
                                     ? transpileWgsl(shader->vertexGlsl, true)
                                     : std::string();
  const std::string fragmentWgsl =
      !m_graphicsShaderOutputDirectory.empty()
          ? transpileWgsl(shader->fragmentGlsl, false)
          : std::string();
  if (!vertexWgsl.empty()) {
    vertexWgslPtr = createGlobalCStringPtr(*m_builder, *m_module, *m_context,
                                           llvm::StringRef(vertexWgsl),
                                           "neuron_shader_wgsl");
    vertexWgslSize = static_cast<uint32_t>(vertexWgsl.size());
  }
  if (!fragmentWgsl.empty()) {
    fragmentWgslPtr =
        createGlobalCStringPtr(*m_builder, *m_module, *m_context,
                               llvm::StringRef(fragmentWgsl),
                               "neuron_shader_wgsl");
    fragmentWgslSize = static_cast<uint32_t>(fragmentWgsl.size());
  }

  if (!m_graphicsShaderOutputDirectory.empty()) {
    const std::string baseName = sanitizeShaderArtifactName(shader->name);
    if (!shader->vertexGlsl.empty()) {
      std::string writeError;
      if (!writeBinaryFile(
              m_graphicsShaderOutputDirectory /
                  (baseName + ".vert.spv"),
              m_graphicsSpirvCache.at("vert:" + shader->vertexGlsl),
              &writeError)) {
        throw std::runtime_error(writeError);
      }
      if (!vertexWgsl.empty() &&
          !writeTextFile(m_graphicsShaderOutputDirectory /
                             (baseName + ".vert.wgsl"),
                         vertexWgsl, &writeError)) {
        throw std::runtime_error(writeError);
      }
    }
    if (!shader->fragmentGlsl.empty()) {
      std::string writeError;
      if (!writeBinaryFile(
              m_graphicsShaderOutputDirectory /
                  (baseName + ".frag.spv"),
              m_graphicsSpirvCache.at("frag:" + shader->fragmentGlsl),
              &writeError)) {
        throw std::runtime_error(writeError);
      }
      if (!fragmentWgsl.empty() &&
          !writeTextFile(m_graphicsShaderOutputDirectory /
                             (baseName + ".frag.wgsl"),
                         fragmentWgsl, &writeError)) {
        throw std::runtime_error(writeError);
      }
    }
  }

  llvm::Constant *shaderNameGlobal = createGlobalCStringPtr(
      *m_builder, *m_module, *m_context, llvm::StringRef(shader->name),
      "shader_name");
  auto *descriptorInit = llvm::ConstantStruct::get(
      descriptorStructType,
      {shaderNameGlobal, llvm::ConstantInt::get(i32Type, stageMask),
       llvm::ConstantInt::get(i32Type,
                              static_cast<uint32_t>(shader->bindings.size())),
       bindingArrayPtr,
       llvm::ConstantInt::get(i32Type, shader->vertexLayoutMask),
       llvm::ConstantInt::get(i32Type, shader->uniformBufferSize),
       llvm::ConstantInt::get(i32Type, shader->mvpOffset),
       vertexSpirvPtr, llvm::ConstantInt::get(i32Type, vertexWordCount),
       fragmentSpirvPtr, llvm::ConstantInt::get(i32Type, fragmentWordCount),
       vertexWgslPtr, llvm::ConstantInt::get(i32Type, vertexWgslSize),
       fragmentWgslPtr, llvm::ConstantInt::get(i32Type, fragmentWgslSize)});
  auto *descriptorGlobal = new llvm::GlobalVariable(
      *m_module, descriptorStructType, true, llvm::GlobalValue::PrivateLinkage,
      descriptorInit, "neuron_shader_descriptor");
  descriptorGlobal->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
  descriptorGlobal->setAlignment(llvm::Align(8));

  m_graphicsShaderDescriptorMap[std::string(shaderName)] = descriptorGlobal;
  return descriptorGlobal;
}

void LLVMCodeGen::declareRuntimeFunctions() {
  codegen::llvm_support::LLVMRuntimeRegistryState state;
  state.context = m_context.get();
  state.module = m_module.get();
  state.runtimeStartupFn = &m_runtimeStartupFn;
  state.runtimeShutdownFn = &m_runtimeShutdownFn;
  state.moduleInitFn = &m_moduleInitFn;
  state.threadSubmitFn = &m_threadSubmitFn;
  state.gpuScopeBeginFn = &m_gpuScopeBeginFn;
  state.gpuScopeBeginExFn = &m_gpuScopeBeginExFn;
  state.gpuScopeEndFn = &m_gpuScopeEndFn;
  state.printIntFn = &m_printIntFn;
  state.printStrFn = &m_printStrFn;
  state.ioWriteLineFn = &m_ioWriteLineFn;
  state.ioReadIntFn = &m_ioReadIntFn;
  state.ioInputIntFn = &m_ioInputIntFn;
  state.ioInputFloatFn = &m_ioInputFloatFn;
  state.ioInputDoubleFn = &m_ioInputDoubleFn;
  state.ioInputBoolFn = &m_ioInputBoolFn;
  state.ioInputStringFn = &m_ioInputStringFn;
  state.ioInputEnumFn = &m_ioInputEnumFn;
  state.timeNowFn = &m_timeNowFn;
  state.randomIntFn = &m_randomIntFn;
  state.randomFloatFn = &m_randomFloatFn;
  state.logInfoFn = &m_logInfoFn;
  state.logWarningFn = &m_logWarningFn;
  state.logErrorFn = &m_logErrorFn;
  state.throwFn = &m_throwFn;
  state.lastExceptionFn = &m_lastExceptionFn;
  state.clearExceptionFn = &m_clearExceptionFn;
  state.hasExceptionFn = &m_hasExceptionFn;
  state.tensorAddFn = &m_tensorAddFn;
  state.tensorSubFn = &m_tensorSubFn;
  state.tensorMulFn = &m_tensorMulFn;
  state.tensorDivFn = &m_tensorDivFn;
  state.tensorFmaFn = &m_tensorFmaFn;
  state.tensorMatMulFn = &m_tensorMatMulFn;
  state.tensorMatMulAddFn = &m_tensorMatMulAddFn;
  state.tensorLinearFusedFn = &m_tensorLinearFusedFn;
  state.tensorConv2DBatchNormReluFn = &m_tensorConv2DBatchNormReluFn;
  state.tensorRandom2DFn = &m_tensorRandom2DFn;
  state.graphicsCreateWindowFn = &m_graphicsCreateWindowFn;
  state.graphicsCreateCanvasFn = &m_graphicsCreateCanvasFn;
  state.graphicsCanvasFreeFn = &m_graphicsCanvasFreeFn;
  state.graphicsCanvasPumpFn = &m_graphicsCanvasPumpFn;
  state.graphicsCanvasShouldCloseFn = &m_graphicsCanvasShouldCloseFn;
  state.graphicsCanvasTakeResizeFn = &m_graphicsCanvasTakeResizeFn;
  state.graphicsCanvasBeginFrameFn = &m_graphicsCanvasBeginFrameFn;
  state.graphicsCanvasEndFrameFn = &m_graphicsCanvasEndFrameFn;
  state.graphicsMaterialCreateFn = &m_graphicsMaterialCreateFn;
  state.graphicsMaterialSetVec4Fn = &m_graphicsMaterialSetVec4Fn;
  state.graphicsMaterialSetTextureFn = &m_graphicsMaterialSetTextureFn;
  state.graphicsMaterialSetSamplerFn = &m_graphicsMaterialSetSamplerFn;
  state.graphicsMaterialSetMatrix4Fn = &m_graphicsMaterialSetMatrix4Fn;
  state.graphicsColorCreateFn = &m_graphicsColorCreateFn;
  state.graphicsVector2CreateFn = &m_graphicsVector2CreateFn;
  state.graphicsVector3CreateFn = &m_graphicsVector3CreateFn;
  state.graphicsVector4CreateFn = &m_graphicsVector4CreateFn;
  state.graphicsTextureLoadFn = &m_graphicsTextureLoadFn;
  state.graphicsSamplerCreateFn = &m_graphicsSamplerCreateFn;
  state.graphicsMeshLoadFn = &m_graphicsMeshLoadFn;
  state.graphicsDrawFn = &m_graphicsDrawFn;
  state.graphicsDrawIndexedFn = &m_graphicsDrawIndexedFn;
  state.graphicsDrawInstancedFn = &m_graphicsDrawInstancedFn;
  state.graphicsClearFn = &m_graphicsClearFn;
  state.graphicsPresentFn = &m_graphicsPresentFn;
  state.graphicsWindowGetWidthFn = &m_graphicsWindowGetWidthFn;
  state.graphicsWindowGetHeightFn = &m_graphicsWindowGetHeightFn;
  state.graphicsLastErrorFn = &m_graphicsLastErrorFn;
  state.graphicsSceneCreateFn = &m_graphicsSceneCreateFn;
  state.graphicsSceneCreateEntityFn = &m_graphicsSceneCreateEntityFn;
  state.graphicsSceneDestroyEntityFn = &m_graphicsSceneDestroyEntityFn;
  state.graphicsSceneFindEntityFn = &m_graphicsSceneFindEntityFn;
  state.graphicsEntityGetTransformFn = &m_graphicsEntityGetTransformFn;
  state.graphicsEntityAddCamera2DFn = &m_graphicsEntityAddCamera2DFn;
  state.graphicsEntityAddSpriteRenderer2DFn =
      &m_graphicsEntityAddSpriteRenderer2DFn;
  state.graphicsEntityAddShapeRenderer2DFn =
      &m_graphicsEntityAddShapeRenderer2DFn;
  state.graphicsEntityAddTextRenderer2DFn =
      &m_graphicsEntityAddTextRenderer2DFn;
  state.graphicsTransformSetParentFn = &m_graphicsTransformSetParentFn;
  state.graphicsTransformSetPositionFn = &m_graphicsTransformSetPositionFn;
  state.graphicsTransformSetRotationFn = &m_graphicsTransformSetRotationFn;
  state.graphicsTransformSetScaleFn = &m_graphicsTransformSetScaleFn;
  state.graphicsRenderer2DCreateFn = &m_graphicsRenderer2DCreateFn;
  state.graphicsRenderer2DSetClearColorFn =
      &m_graphicsRenderer2DSetClearColorFn;
  state.graphicsRenderer2DSetCameraFn = &m_graphicsRenderer2DSetCameraFn;
  state.graphicsRenderer2DRenderFn = &m_graphicsRenderer2DRenderFn;
  state.graphicsCamera2DSetZoomFn = &m_graphicsCamera2DSetZoomFn;
  state.graphicsCamera2DSetPrimaryFn = &m_graphicsCamera2DSetPrimaryFn;
  state.graphicsSpriteRenderer2DSetTextureFn =
      &m_graphicsSpriteRenderer2DSetTextureFn;
  state.graphicsSpriteRenderer2DSetColorFn =
      &m_graphicsSpriteRenderer2DSetColorFn;
  state.graphicsSpriteRenderer2DSetSizeFn =
      &m_graphicsSpriteRenderer2DSetSizeFn;
  state.graphicsSpriteRenderer2DSetPivotFn =
      &m_graphicsSpriteRenderer2DSetPivotFn;
  state.graphicsSpriteRenderer2DSetFlipXFn =
      &m_graphicsSpriteRenderer2DSetFlipXFn;
  state.graphicsSpriteRenderer2DSetFlipYFn =
      &m_graphicsSpriteRenderer2DSetFlipYFn;
  state.graphicsSpriteRenderer2DSetSortingLayerFn =
      &m_graphicsSpriteRenderer2DSetSortingLayerFn;
  state.graphicsSpriteRenderer2DSetOrderInLayerFn =
      &m_graphicsSpriteRenderer2DSetOrderInLayerFn;
  state.graphicsShapeRenderer2DSetRectangleFn =
      &m_graphicsShapeRenderer2DSetRectangleFn;
  state.graphicsShapeRenderer2DSetCircleFn =
      &m_graphicsShapeRenderer2DSetCircleFn;
  state.graphicsShapeRenderer2DSetLineFn = &m_graphicsShapeRenderer2DSetLineFn;
  state.graphicsShapeRenderer2DSetColorFn =
      &m_graphicsShapeRenderer2DSetColorFn;
  state.graphicsShapeRenderer2DSetFilledFn =
      &m_graphicsShapeRenderer2DSetFilledFn;
  state.graphicsShapeRenderer2DSetSortingLayerFn =
      &m_graphicsShapeRenderer2DSetSortingLayerFn;
  state.graphicsShapeRenderer2DSetOrderInLayerFn =
      &m_graphicsShapeRenderer2DSetOrderInLayerFn;
  state.graphicsFontLoadFn = &m_graphicsFontLoadFn;
  state.graphicsTextRenderer2DSetFontFn = &m_graphicsTextRenderer2DSetFontFn;
  state.graphicsTextRenderer2DSetTextFn = &m_graphicsTextRenderer2DSetTextFn;
  state.graphicsTextRenderer2DSetFontSizeFn =
      &m_graphicsTextRenderer2DSetFontSizeFn;
  state.graphicsTextRenderer2DSetColorFn =
      &m_graphicsTextRenderer2DSetColorFn;
  state.graphicsTextRenderer2DSetAlignmentFn =
      &m_graphicsTextRenderer2DSetAlignmentFn;
  state.graphicsTextRenderer2DSetSortingLayerFn =
      &m_graphicsTextRenderer2DSetSortingLayerFn;
  state.graphicsTextRenderer2DSetOrderInLayerFn =
      &m_graphicsTextRenderer2DSetOrderInLayerFn;
  state.nnSelfTestFn = &m_nnSelfTestFn;
  state.moduleCppRegisterFn = &m_moduleCppRegisterFn;
  state.moduleCppStartupFn = &m_moduleCppStartupFn;
  state.moduleCppShutdownFn = &m_moduleCppShutdownFn;
  state.moduleCppInvokeFn = &m_moduleCppInvokeFn;
  codegen::llvm_support::declareRuntimeFunctions(state);
}

void LLVMCodeGen::generate(nir::Module *nirModule) {
  m_sourceModule = nirModule;
  m_graphicsShaderDescriptorMap.clear();
  m_graphicsWgslCache.clear();
  m_module->setModuleIdentifier(nirModule->getName());

  declareRuntimeFunctions();

  // Create class types (forward declarations)
  for (const auto &cls : nirModule->getClasses()) {
    auto *structType = llvm::StructType::create(*m_context, cls->getName());
    m_structMap[cls->getName()] = structType;
    m_classMap[cls->getName()] = cls.get();
  }

  // Populate class types (layouts)
  for (const auto &cls : nirModule->getClasses()) {
    auto *structType = m_structMap[cls->getName()];
    std::vector<llvm::Type *> fieldTypes;
    for (const auto &field : cls->getFields()) {
      fieldTypes.push_back(toLLVMType(field.type));
    }
    structType->setBody(fieldTypes);
  }

  // Create Global Variables
  for (const auto &global : nirModule->getGlobals()) {
    llvm::Type *gType = toLLVMType(global->getType());
    auto *llGlobal = new llvm::GlobalVariable(
        *m_module, gType, false, llvm::GlobalValue::InternalLinkage, nullptr,
        global->getName());

    // Simple initializers for constants if available
    llvm::Constant *initConst = llvm::Constant::getNullValue(gType);
    if (global->getInitializer()) {
      if (global->getInitializer()->getValueKind() ==
          nir::ValueKind::ConstantInt) {
        initConst = llvm::ConstantInt::get(
            gType, static_cast<nir::ConstantInt *>(global->getInitializer())
                       ->getValue());
      } else if (global->getInitializer()->getValueKind() ==
                 nir::ValueKind::ConstantFloat) {
        initConst = llvm::ConstantFP::get(
            gType, static_cast<nir::ConstantFloat *>(global->getInitializer())
                       ->getValue());
      }
    }
    llGlobal->setInitializer(initConst);
    m_valueMap[global.get()] = llGlobal;
  }

  // First pass: create all function prototypes
  for (const auto &func : nirModule->getFunctions()) {
    std::vector<llvm::Type *> paramTypes;
    for (const auto &arg : func->getArguments()) {
      paramTypes.push_back(toLLVMType(arg->getType()));
    }

    llvm::Type *retType = toLLVMType(func->getReturnType());
    auto *fnType = llvm::FunctionType::get(retType, paramTypes, false);
    auto *llFunc =
        llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                               func->getName(), m_module.get());
    m_functionMap[func->getName()] = llFunc;

    // Map arguments
    size_t i = 0;
    for (auto &llArg : llFunc->args()) {
      if (i < func->getArguments().size()) {
        llArg.setName(func->getArguments()[i]->getName());
        m_valueMap[func->getArguments()[i].get()] = &llArg;
      }
      i++;
    }
  }

  // Second pass: emit function bodies
  for (const auto &func : nirModule->getFunctions()) {
    if (func->isExtern()) {
      continue;
    }
    emitFunction(func.get());
  }

  // Create main() only for the zero-argument configured entry point.
  if (m_functionMap.count(m_entryFunctionName) &&
      m_functionMap[m_entryFunctionName]->arg_empty()) {
    auto *mainType =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*m_context), {}, false);
    auto *mainFn = llvm::Function::Create(
        mainType, llvm::Function::ExternalLinkage, "main", m_module.get());
    auto *mainBB = llvm::BasicBlock::Create(*m_context, "entry", mainFn);
    m_builder->SetInsertPoint(mainBB);

    if (m_runtimeStartupFn) {
      m_builder->CreateCall(m_runtimeStartupFn);
    }
    if (m_moduleInitFn) {
      m_builder->CreateCall(
          m_moduleInitFn,
          {createGlobalCStringPtr(*m_builder, *m_module, *m_context,
                                  m_module->getModuleIdentifier())});
    }
    if (!m_moduleCppExports.empty() && m_moduleCppRegisterFn != nullptr) {
      std::vector<std::string> callTargets;
      callTargets.reserve(m_moduleCppExports.size());
      for (const auto &entry : m_moduleCppExports) {
        callTargets.push_back(entry.first);
      }
      std::sort(callTargets.begin(), callTargets.end());
      for (const auto &callTarget : callTargets) {
        const ModuleCppCompileExport &entry = m_moduleCppExports.at(callTarget);
        m_builder->CreateCall(
            m_moduleCppRegisterFn,
            {createGlobalCStringPtr(*m_builder, *m_module, *m_context,
                                    callTarget),
             createGlobalCStringPtr(*m_builder, *m_module, *m_context,
                                    entry.libraryPath),
             createGlobalCStringPtr(*m_builder, *m_module, *m_context,
                                    entry.symbolName),
             createGlobalCStringPtr(*m_builder, *m_module, *m_context,
                                    joinParameterTypes(entry.parameterTypes)),
             createGlobalCStringPtr(*m_builder, *m_module, *m_context,
                                    entry.returnType)});
      }
    }
    if (!m_moduleCppExports.empty() && m_moduleCppStartupFn != nullptr) {
      llvm::Value *startupOk = m_builder->CreateCall(m_moduleCppStartupFn);
      llvm::Value *startupPassed = m_builder->CreateICmpNE(
          startupOk,
          llvm::ConstantInt::get(llvm::Type::getInt64Ty(*m_context), 0));
      auto *startupCont =
          llvm::BasicBlock::Create(*m_context, "modulecpp.startup.ok", mainFn);
      auto *startupFail = llvm::BasicBlock::Create(
          *m_context, "modulecpp.startup.fail", mainFn);
      m_builder->CreateCondBr(startupPassed, startupCont, startupFail);
      m_builder->SetInsertPoint(startupFail);
      m_builder->CreateRet(
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), 1));
      m_builder->SetInsertPoint(startupCont);
    }
    m_builder->CreateCall(m_functionMap[m_entryFunctionName]);
    if (!m_moduleCppExports.empty() && m_moduleCppShutdownFn != nullptr) {
      m_builder->CreateCall(m_moduleCppShutdownFn);
    }
    if (m_runtimeShutdownFn) {
      m_builder->CreateCall(m_runtimeShutdownFn);
    }
    m_builder->CreateRet(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), 0));
  }
}

void LLVMCodeGen::emitFunction(nir::Function *func) {
  llvm::Function *llFunc = m_functionMap[func->getName()];
  if (!llFunc)
    return;

  // Clear per-function block map
  m_blockMap.clear();

  // Create basic blocks
  for (const auto &block : func->getBlocks()) {
    auto *bb = llvm::BasicBlock::Create(*m_context, block->getName(), llFunc);
    m_blockMap[block->getName()] = bb;
  }

  // Emit instructions for each block
  for (const auto &block : func->getBlocks()) {
    m_builder->SetInsertPoint(m_blockMap[block->getName()]);

    for (const auto &inst : block->getInstructions()) {
      // Skip instructions if the block already has a terminator
      if (m_blockMap[block->getName()]->getTerminator())
        break;
      emitInstruction(inst.get());
    }

    // If block doesn't have a terminator, add one
    if (!m_blockMap[block->getName()]->getTerminator()) {
      llvm::Type *retType = llFunc->getReturnType();
      if (retType->isVoidTy()) {
        m_builder->CreateRetVoid();
      } else {
        m_builder->CreateRet(defaultValueForType(retType));
      }
    }
  }
}

void LLVMCodeGen::emitInstruction(nir::Instruction *inst) {
  switch (inst->getKind()) {
  case nir::InstKind::Add: {
    llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
    llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
    if (lhs && rhs) {
      auto *result = m_builder->CreateAdd(lhs, rhs, inst->getName());
      m_valueMap[inst] = result;
    }
    break;
  }
  case nir::InstKind::Sub: {
    llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
    llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
    if (lhs && rhs) {
      auto *result = m_builder->CreateSub(lhs, rhs, inst->getName());
      m_valueMap[inst] = result;
    }
    break;
  }
  case nir::InstKind::Mul: {
    llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
    llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
    if (lhs && rhs) {
      auto *result = m_builder->CreateMul(lhs, rhs, inst->getName());
      m_valueMap[inst] = result;
    }
    break;
  }
  case nir::InstKind::Div: {
    llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
    llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
    if (lhs && rhs) {
      auto *result = m_builder->CreateSDiv(lhs, rhs, inst->getName());
      m_valueMap[inst] = result;
    }
    break;
  }
  case nir::InstKind::Pow: {
    llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
    llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
    if (lhs && rhs) {
      if (!lhs->getType()->isFloatingPointTy()) {
        lhs = m_builder->CreateSIToFP(lhs, llvm::Type::getDoubleTy(*m_context));
      }
      if (!rhs->getType()->isFloatingPointTy()) {
        rhs = m_builder->CreateSIToFP(rhs, llvm::Type::getDoubleTy(*m_context));
      }
      auto *powFn = llvm::Intrinsic::getDeclaration(
          m_module.get(), llvm::Intrinsic::pow, {lhs->getType()});
      m_valueMap[inst] = m_builder->CreateCall(powFn, {lhs, rhs}, inst->getName());
    }
    break;
  }
  case nir::InstKind::NthRoot: {
    llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
    llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
    if (lhs && rhs) {
      if (!lhs->getType()->isFloatingPointTy()) {
        lhs = m_builder->CreateSIToFP(lhs, llvm::Type::getDoubleTy(*m_context));
      }
      if (!rhs->getType()->isFloatingPointTy()) {
        rhs = m_builder->CreateSIToFP(rhs, llvm::Type::getDoubleTy(*m_context));
      }
      llvm::Value *one = llvm::ConstantFP::get(lhs->getType(), 1.0);
      llvm::Value *inv = m_builder->CreateFDiv(one, rhs, inst->getName() + ".inv");
      auto *powFn = llvm::Intrinsic::getDeclaration(
          m_module.get(), llvm::Intrinsic::pow, {lhs->getType()});
      m_valueMap[inst] = m_builder->CreateCall(powFn, {lhs, inv}, inst->getName());
    }
    break;
  }
  case nir::InstKind::Sqrt: {
    llvm::Value *arg = toLLVMValue(inst->getOperand(0));
    if (arg) {
      if (!arg->getType()->isFloatingPointTy()) {
        arg = m_builder->CreateSIToFP(arg, llvm::Type::getDoubleTy(*m_context));
      }
      auto *sqrtFn = llvm::Intrinsic::getDeclaration(
          m_module.get(), llvm::Intrinsic::sqrt, {arg->getType()});
      m_valueMap[inst] = m_builder->CreateCall(sqrtFn, {arg}, inst->getName());
    }
    break;
  }
  case nir::InstKind::Eq:
  case nir::InstKind::Neq:
  case nir::InstKind::Lt:
  case nir::InstKind::Lte:
  case nir::InstKind::Gt:
  case nir::InstKind::Gte: {
    llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
    llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
    if (lhs && rhs) {
      llvm::CmpInst::Predicate pred;
      switch (inst->getKind()) {
      case nir::InstKind::Eq:
        pred = llvm::CmpInst::ICMP_EQ;
        break;
      case nir::InstKind::Neq:
        pred = llvm::CmpInst::ICMP_NE;
        break;
      case nir::InstKind::Lt:
        pred = llvm::CmpInst::ICMP_SLT;
        break;
      case nir::InstKind::Lte:
        pred = llvm::CmpInst::ICMP_SLE;
        break;
      case nir::InstKind::Gt:
        pred = llvm::CmpInst::ICMP_SGT;
        break;
      case nir::InstKind::Gte:
        pred = llvm::CmpInst::ICMP_SGE;
        break;
      default:
        pred = llvm::CmpInst::ICMP_EQ;
        break;
      }
      auto *result = m_builder->CreateICmp(pred, lhs, rhs, inst->getName());
      m_valueMap[inst] = result;
    }
    break;
  }
  case nir::InstKind::Alloca: {
    // Alloca allocates space for a local variable
    llvm::Type *allocType = llvm::Type::getInt64Ty(*m_context); // default
    if (inst->getType() && inst->getType()->kind == TypeKind::Pointer &&
        inst->getType()->pointeeType) {
      allocType = toLLVMType(inst->getType()->pointeeType);
    }
    auto *alloca = m_builder->CreateAlloca(allocType, nullptr, inst->getName());
    m_valueMap[inst] = alloca;
    break;
  }
  case nir::InstKind::Store: {
    if (inst->getOperands().size() >= 2) {
      llvm::Value *val = toLLVMValue(inst->getOperand(0));
      llvm::Value *ptr = toLLVMValue(inst->getOperand(1));
      if (val && ptr) {
        // Ensure types match
        llvm::Type *ptrElemType = llvm::Type::getInt64Ty(*m_context);
        if (auto *allocaInst = llvm::dyn_cast<llvm::AllocaInst>(ptr)) {
          ptrElemType = allocaInst->getAllocatedType();
        }
        if (val->getType() != ptrElemType) {
          // Try to cast
          if (val->getType()->isIntegerTy() && ptrElemType->isIntegerTy()) {
            val = m_builder->CreateIntCast(val, ptrElemType, true);
          }
        }
        m_builder->CreateStore(val, ptr);
      }
    }
    break;
  }
  case nir::InstKind::Load: {
    if (inst->getOperands().size() >= 1) {
      llvm::Value *ptr = toLLVMValue(inst->getOperand(0));
      if (ptr) {
        llvm::Type *loadType = llvm::Type::getInt64Ty(*m_context);
        if (auto *allocaInst = llvm::dyn_cast<llvm::AllocaInst>(ptr)) {
          loadType = allocaInst->getAllocatedType();
        } else if (auto *globalVar =
                       llvm::dyn_cast<llvm::GlobalVariable>(ptr)) {
          loadType = globalVar->getValueType();
        }
        auto *result = m_builder->CreateLoad(loadType, ptr, inst->getName());
        m_valueMap[inst] = result;
      }
    }
    break;
  }
  case nir::InstKind::Call: {
    if (inst->getOperands().size() >= 1) {
      // Check if the callee is a named function (ConstantString)
      if (inst->getOperand(0)->getValueKind() ==
          nir::ValueKind::ConstantString) {
        auto *cs = static_cast<nir::ConstantString *>(inst->getOperand(0));
        std::string funcName = cs->getValue();

        if (funcName == "Print" && inst->getOperands().size() >= 2) {
          // Built-in Print
          llvm::Value *arg = toLLVMValue(inst->getOperand(1));
          if (arg) {
            if (arg->getType()->isIntegerTy()) {
              m_builder->CreateCall(m_printIntFn, {arg});
            } else if (arg->getType()->isFloatingPointTy()) {
              // Handle float print (mock: print as int for now or add float
              // print)
              m_builder->CreateCall(
                  m_printIntFn, {m_builder->CreateFPToSI(
                                    arg, llvm::Type::getInt64Ty(*m_context))});
            } else {
              m_builder->CreateCall(m_printStrFn, {arg});
            }
          }
        } else if (funcName == "__repl_echo_string" &&
                   inst->getOperands().size() >= 2) {
          llvm::Value *arg = toLLVMValue(inst->getOperand(1));
          if (arg) {
            if (!arg->getType()->isPointerTy()) {
              arg = llvm::ConstantPointerNull::get(
                  llvm::PointerType::get(*m_context, 0));
            }
            auto replEchoFn = m_module->getOrInsertFunction(
                "neuron_repl_echo_string",
                llvm::FunctionType::get(llvm::Type::getVoidTy(*m_context),
                                        {llvm::PointerType::get(*m_context, 0)},
                                        false));
            m_builder->CreateCall(replEchoFn, {arg});
          }
        } else if (funcName == "System.Print" &&
                   inst->getOperands().size() >= 2) {
          llvm::Value *arg = toLLVMValue(inst->getOperand(1));
          if (arg) {
            if (arg->getType()->isIntegerTy()) {
              if (arg->getType() != llvm::Type::getInt64Ty(*m_context)) {
                arg = m_builder->CreateIntCast(
                    arg, llvm::Type::getInt64Ty(*m_context), true);
              }
              m_builder->CreateCall(m_printIntFn, {arg});
            } else if (arg->getType()->isFloatingPointTy()) {
              llvm::Value *asInt = m_builder->CreateFPToSI(
                  arg, llvm::Type::getInt64Ty(*m_context));
              m_builder->CreateCall(m_printIntFn, {asInt});
            } else {
              m_builder->CreateCall(m_printStrFn, {arg});
            }
          }
        } else if (funcName == "IO.WriteLine" &&
                   inst->getOperands().size() >= 2) {
          llvm::Value *arg = toLLVMValue(inst->getOperand(1));
          if (arg) {
            m_builder->CreateCall(m_ioWriteLineFn, {arg});
          }
        } else if (funcName == "IO.ReadInt") {
          m_valueMap[inst] = m_builder->CreateCall(m_ioReadIntFn, {});
        } else if (funcName == "__neuron_input_int" &&
                   m_ioInputIntFn != nullptr) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i64Ty = llvm::Type::getInt64Ty(*m_context);
          auto getPtrArg = [&](size_t operandIndex) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                value = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            return value;
          };
          auto getI64Arg = [&](size_t operandIndex,
                               int64_t fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantInt::get(i64Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isIntegerTy(64)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i64Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i64Ty);
              } else {
                value = llvm::ConstantInt::get(i64Ty, fallback);
              }
            }
            return value;
          };
          m_valueMap[inst] = m_builder->CreateCall(
              m_ioInputIntFn,
              {getPtrArg(1), getI64Arg(2, 0), getI64Arg(3, 0), getI64Arg(4, 0),
               getI64Arg(5, 0), getI64Arg(6, 0), getI64Arg(7, 0),
               getI64Arg(8, -1)},
              inst->getName());
        } else if (funcName == "__neuron_input_float" &&
                   m_ioInputFloatFn != nullptr) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i64Ty = llvm::Type::getInt64Ty(*m_context);
          auto *f32Ty = llvm::Type::getFloatTy(*m_context);
          auto getPtrArg = [&](size_t operandIndex) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                value = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            return value;
          };
          auto getI64Arg = [&](size_t operandIndex,
                               int64_t fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantInt::get(i64Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isIntegerTy(64)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i64Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i64Ty);
              } else {
                value = llvm::ConstantInt::get(i64Ty, fallback);
              }
            }
            return value;
          };
          auto getF32Arg = [&](size_t operandIndex,
                               float fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantFP::get(f32Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isFloatTy()) {
              if (value->getType()->isDoubleTy()) {
                value = m_builder->CreateFPTrunc(value, f32Ty);
              } else if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateSIToFP(value, f32Ty);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPTrunc(value, f32Ty);
              } else {
                value = llvm::ConstantFP::get(f32Ty, fallback);
              }
            }
            return value;
          };
          m_valueMap[inst] = m_builder->CreateCall(
              m_ioInputFloatFn,
              {getPtrArg(1), getI64Arg(2, 0), getF32Arg(3, 0.0f),
               getI64Arg(4, 0), getF32Arg(5, 0.0f), getI64Arg(6, 0),
               getF32Arg(7, 0.0f), getI64Arg(8, -1)},
              inst->getName());
        } else if (funcName == "__neuron_input_double" &&
                   m_ioInputDoubleFn != nullptr) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i64Ty = llvm::Type::getInt64Ty(*m_context);
          auto *f64Ty = llvm::Type::getDoubleTy(*m_context);
          auto getPtrArg = [&](size_t operandIndex) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                value = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            return value;
          };
          auto getI64Arg = [&](size_t operandIndex,
                               int64_t fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantInt::get(i64Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isIntegerTy(64)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i64Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i64Ty);
              } else {
                value = llvm::ConstantInt::get(i64Ty, fallback);
              }
            }
            return value;
          };
          auto getF64Arg = [&](size_t operandIndex,
                               double fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantFP::get(f64Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isDoubleTy()) {
              if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPExt(value, f64Ty);
              } else if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateSIToFP(value, f64Ty);
              } else {
                value = llvm::ConstantFP::get(f64Ty, fallback);
              }
            }
            return value;
          };
          m_valueMap[inst] = m_builder->CreateCall(
              m_ioInputDoubleFn,
              {getPtrArg(1), getI64Arg(2, 0), getF64Arg(3, 0.0),
               getI64Arg(4, 0), getF64Arg(5, 0.0), getI64Arg(6, 0),
               getF64Arg(7, 0.0), getI64Arg(8, -1)},
              inst->getName());
        } else if (funcName == "__neuron_input_bool" &&
                   m_ioInputBoolFn != nullptr) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i64Ty = llvm::Type::getInt64Ty(*m_context);
          auto getPtrArg = [&](size_t operandIndex) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                value = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            return value;
          };
          auto getI64Arg = [&](size_t operandIndex,
                               int64_t fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantInt::get(i64Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isIntegerTy(64)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i64Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i64Ty);
              } else {
                value = llvm::ConstantInt::get(i64Ty, fallback);
              }
            }
            return value;
          };
          llvm::Value *raw = m_builder->CreateCall(
              m_ioInputBoolFn,
              {getPtrArg(1), getI64Arg(2, 0), getI64Arg(3, 0), getI64Arg(4, -1)});
          if (inst->getType() && inst->getType()->kind == TypeKind::Bool) {
            raw = m_builder->CreateICmpNE(
                raw, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*m_context), 0));
          }
          m_valueMap[inst] = raw;
        } else if (funcName == "__neuron_input_string" &&
                   m_ioInputStringFn != nullptr) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i64Ty = llvm::Type::getInt64Ty(*m_context);
          auto getPtrArg = [&](size_t operandIndex) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                value = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            return value;
          };
          auto getI64Arg = [&](size_t operandIndex,
                               int64_t fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantInt::get(i64Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isIntegerTy(64)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i64Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i64Ty);
              } else {
                value = llvm::ConstantInt::get(i64Ty, fallback);
              }
            }
            return value;
          };
          m_valueMap[inst] = m_builder->CreateCall(
              m_ioInputStringFn,
              {getPtrArg(1), getI64Arg(2, 0), getI64Arg(3, 0), getPtrArg(4),
               getI64Arg(5, -1)},
              inst->getName());
        } else if (funcName == "__neuron_input_enum" &&
                   m_ioInputEnumFn != nullptr) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i64Ty = llvm::Type::getInt64Ty(*m_context);
          auto getPtrArg = [&](size_t operandIndex) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                value = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            return value;
          };
          auto getI64Arg = [&](size_t operandIndex,
                               int64_t fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantInt::get(i64Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isIntegerTy(64)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i64Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i64Ty);
              } else {
                value = llvm::ConstantInt::get(i64Ty, fallback);
              }
            }
            return value;
          };
          m_valueMap[inst] = m_builder->CreateCall(
              m_ioInputEnumFn,
              {getPtrArg(1), getPtrArg(2), getI64Arg(3, 0), getI64Arg(4, 0),
               getI64Arg(5, 0), getI64Arg(6, -1)},
              inst->getName());
        } else if (funcName == "thread" && m_threadSubmitFn != nullptr) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *entryFn = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *userData = llvm::ConstantPointerNull::get(ptrTy);

          std::string targetName;
          if (inst->getOperands().size() >= 2 &&
              inst->getOperand(1)->getValueKind() ==
                  nir::ValueKind::ConstantString) {
            targetName =
                static_cast<nir::ConstantString *>(inst->getOperand(1))->getValue();
          }

          if (!targetName.empty()) {
            auto fnIt = m_functionMap.find(targetName);
            if (fnIt != m_functionMap.end() && fnIt->second != nullptr) {
              llvm::Function *targetFn = fnIt->second;
              auto thunkIt = m_threadThunkMap.find(targetName);
              llvm::Function *thunkFn = nullptr;
              if (thunkIt != m_threadThunkMap.end()) {
                thunkFn = thunkIt->second;
              } else {
                auto *thunkType = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(*m_context), {ptrTy}, false);
                thunkFn = llvm::Function::Create(
                    thunkType, llvm::Function::InternalLinkage,
                    makeThreadThunkName(targetName), m_module.get());
                llvm::BasicBlock *thunkEntry =
                    llvm::BasicBlock::Create(*m_context, "entry", thunkFn);
                llvm::BasicBlock *savedInsertBlock = m_builder->GetInsertBlock();
                m_builder->SetInsertPoint(thunkEntry);

                std::vector<llvm::Value *> thunkArgs;
                if (targetFn->arg_size() == 1) {
                  llvm::Value *arg0 = &*thunkFn->arg_begin();
                  llvm::Type *targetArgType =
                      targetFn->getFunctionType()->getParamType(0);
                  if (arg0->getType() != targetArgType &&
                      arg0->getType()->isPointerTy() &&
                      targetArgType->isPointerTy()) {
                    arg0 = m_builder->CreatePointerCast(arg0, targetArgType);
                  }
                  if (arg0->getType() == targetArgType) {
                    thunkArgs.push_back(arg0);
                  }
                }
                if (targetFn->arg_size() == 0 ||
                    (targetFn->arg_size() == 1 && thunkArgs.size() == 1)) {
                  (void)m_builder->CreateCall(targetFn, thunkArgs);
                }
                m_builder->CreateRetVoid();

                if (savedInsertBlock != nullptr) {
                  m_builder->SetInsertPoint(savedInsertBlock);
                }
                m_threadThunkMap[targetName] = thunkFn;
              }
              entryFn = m_builder->CreatePointerCast(thunkFn, ptrTy);
            }
          } else if (inst->getOperands().size() >= 2) {
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
              if (arg->getType()->isPointerTy()) {
                entryFn = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
          }

          if (inst->getOperands().size() >= 3) {
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
              if (arg->getType()->isPointerTy()) {
                userData = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
          }

          m_builder->CreateCall(m_threadSubmitFn, {entryFn, userData});
          if (inst->getType() != nullptr &&
              inst->getType()->kind == TypeKind::Int) {
            m_valueMap[inst] =
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(*m_context), 0);
          }
        } else if (funcName == "Tensor.Random" &&
                   m_tensorRandom2DFn != nullptr) {
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          auto toI32Arg = [&](size_t operandIndex, int32_t fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantInt::get(i32Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isIntegerTy(32)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i32Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i32Ty);
              } else {
                value = llvm::ConstantInt::get(i32Ty, fallback);
              }
            }
            return value;
          };
          m_valueMap[inst] = m_builder->CreateCall(
              m_tensorRandom2DFn, {toI32Arg(1, 1), toI32Arg(2, 1)},
              inst->getName());
        } else if (funcName == "__neuron_fused_conv2d_batchnorm_relu" &&
                   m_tensorConv2DBatchNormReluFn != nullptr) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *f32Ty = llvm::Type::getFloatTy(*m_context);
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          auto getPtrArg = [&](size_t operandIndex) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                value = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            return value;
          };
          auto getF32Arg = [&](size_t operandIndex, float fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantFP::get(f32Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isFloatTy()) {
              if (value->getType()->isDoubleTy()) {
                value = m_builder->CreateFPTrunc(value, f32Ty);
              } else if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateSIToFP(value, f32Ty);
              } else {
                value = llvm::ConstantFP::get(f32Ty, fallback);
              }
            }
            return value;
          };
          auto getI32Arg = [&](size_t operandIndex, int32_t fallback) -> llvm::Value * {
            llvm::Value *value = llvm::ConstantInt::get(i32Ty, fallback);
            if (inst->getOperands().size() > operandIndex) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(operandIndex));
              if (arg != nullptr) {
                value = arg;
              }
            }
            if (!value->getType()->isIntegerTy(32)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i32Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i32Ty);
              } else {
                value = llvm::ConstantInt::get(i32Ty, fallback);
              }
            }
            return value;
          };
          m_valueMap[inst] = m_builder->CreateCall(
              m_tensorConv2DBatchNormReluFn,
              {getPtrArg(1), getPtrArg(2), getPtrArg(3), getPtrArg(4),
               getPtrArg(5), getPtrArg(6), getPtrArg(7), getF32Arg(8, 0.001f),
               getI32Arg(9, 1), getI32Arg(10, 1), getI32Arg(11, 0),
               getI32Arg(12, 0), getI32Arg(13, 0)},
              inst->getName());
        } else if (funcName == "Math.Sqrt" && inst->getOperands().size() >= 2) {
          llvm::Value *arg = toLLVMValue(inst->getOperand(1));
          if (arg) {
            if (arg->getType()->isIntegerTy()) {
              arg = m_builder->CreateSIToFP(arg, llvm::Type::getDoubleTy(*m_context));
            }
            auto *sqrtFn = llvm::Intrinsic::getDeclaration(
                m_module.get(), llvm::Intrinsic::sqrt, {arg->getType()});
            m_valueMap[inst] = m_builder->CreateCall(sqrtFn, {arg}, inst->getName());
          }
        } else if (funcName == "Math.Abs" && inst->getOperands().size() >= 2) {
          llvm::Value *arg = toLLVMValue(inst->getOperand(1));
          if (arg) {
            if (arg->getType()->isIntegerTy()) {
              arg = m_builder->CreateSIToFP(arg, llvm::Type::getDoubleTy(*m_context));
            }
            auto *fabsFn = llvm::Intrinsic::getDeclaration(
                m_module.get(), llvm::Intrinsic::fabs, {arg->getType()});
            m_valueMap[inst] = m_builder->CreateCall(fabsFn, {arg}, inst->getName());
          }
        } else if (funcName == "Math.Pow" && inst->getOperands().size() >= 3) {
          llvm::Value *base = toLLVMValue(inst->getOperand(1));
          llvm::Value *exp = toLLVMValue(inst->getOperand(2));
          if (base && exp) {
            if (base->getType()->isIntegerTy()) {
              base = m_builder->CreateSIToFP(base, llvm::Type::getDoubleTy(*m_context));
            }
            if (exp->getType()->isIntegerTy()) {
              exp = m_builder->CreateSIToFP(exp, llvm::Type::getDoubleTy(*m_context));
            }
            auto *powFn = llvm::Intrinsic::getDeclaration(
                m_module.get(), llvm::Intrinsic::pow, {base->getType()});
            m_valueMap[inst] = m_builder->CreateCall(powFn, {base, exp}, inst->getName());
          }
        } else if (funcName == "Time.Now") {
          m_valueMap[inst] = m_builder->CreateCall(m_timeNowFn, {});
        } else if (funcName == "Random.Int" &&
                   inst->getOperands().size() >= 3) {
          llvm::Value *minVal = toLLVMValue(inst->getOperand(1));
          llvm::Value *maxVal = toLLVMValue(inst->getOperand(2));
          if (minVal && maxVal) {
            if (!minVal->getType()->isIntegerTy(64)) {
              minVal = m_builder->CreateIntCast(
                  minVal, llvm::Type::getInt64Ty(*m_context), true);
            }
            if (!maxVal->getType()->isIntegerTy(64)) {
              maxVal = m_builder->CreateIntCast(
                  maxVal, llvm::Type::getInt64Ty(*m_context), true);
            }
            m_valueMap[inst] =
                m_builder->CreateCall(m_randomIntFn, {minVal, maxVal});
          }
        } else if (funcName == "Random.Float") {
          m_valueMap[inst] = m_builder->CreateCall(m_randomFloatFn, {});
        } else if (funcName == "Window.Create" ||
                   funcName == "Graphics.CreateWindow") {
          if (m_graphicsCreateWindowFn != nullptr) {
            auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *width = llvm::ConstantInt::get(i32Ty, 1280);
            llvm::Value *height = llvm::ConstantInt::get(i32Ty, 720);
            llvm::Value *title = llvm::ConstantPointerNull::get(ptrTy);

            if (inst->getOperands().size() >= 2) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(1));
              if (arg != nullptr && arg->getType()->isIntegerTy()) {
                width = m_builder->CreateIntCast(arg, i32Ty, true);
              }
            }
            if (inst->getOperands().size() >= 3) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(2));
              if (arg != nullptr && arg->getType()->isIntegerTy()) {
                height = m_builder->CreateIntCast(arg, i32Ty, true);
              }
            }
            if (inst->getOperands().size() >= 4) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(3));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                title = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }

            m_valueMap[inst] = m_builder->CreateCall(m_graphicsCreateWindowFn,
                                                     {width, height, title});
          }
        } else if (funcName == "Graphics.CreateCanvas") {
          if (m_graphicsCreateCanvasFn != nullptr) {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *window = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() >= 2) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(1));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                window = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_valueMap[inst] =
                m_builder->CreateCall(m_graphicsCreateCanvasFn, {window});
          }
        } else if (funcName == "__neuron_graphics_canvas_free") {
          if (m_graphicsCanvasFreeFn != nullptr) {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *canvas = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() >= 2) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(1));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                canvas = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_builder->CreateCall(m_graphicsCanvasFreeFn, {canvas});
          }
        } else if (funcName == "__neuron_graphics_canvas_pump") {
          if (m_graphicsCanvasPumpFn != nullptr) {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *canvas = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() >= 2) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(1));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                canvas = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_valueMap[inst] =
                m_builder->CreateCall(m_graphicsCanvasPumpFn, {canvas});
          }
        } else if (funcName == "__neuron_graphics_canvas_should_close") {
          if (m_graphicsCanvasShouldCloseFn != nullptr) {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *canvas = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() >= 2) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(1));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                canvas = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_valueMap[inst] =
                m_builder->CreateCall(m_graphicsCanvasShouldCloseFn, {canvas});
          }
        } else if (funcName == "__neuron_graphics_canvas_take_resize") {
          if (m_graphicsCanvasTakeResizeFn != nullptr) {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *canvas = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() >= 2) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(1));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                canvas = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_valueMap[inst] =
                m_builder->CreateCall(m_graphicsCanvasTakeResizeFn, {canvas});
          }
        } else if (funcName == "__neuron_graphics_canvas_begin_frame") {
          if (m_graphicsCanvasBeginFrameFn != nullptr) {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *canvas = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() >= 2) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(1));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                canvas = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_builder->CreateCall(m_graphicsCanvasBeginFrameFn, {canvas});
          }
        } else if (funcName == "__neuron_graphics_canvas_end_frame") {
          if (m_graphicsCanvasEndFrameFn != nullptr) {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *canvas = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() >= 2) {
              llvm::Value *arg = toLLVMValue(inst->getOperand(1));
              if (arg != nullptr && arg->getType()->isPointerTy()) {
                canvas = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_builder->CreateCall(m_graphicsCanvasEndFrameFn, {canvas});
          }
        } else if (funcName == "Material.Create") {
          if (m_graphicsMaterialCreateFn != nullptr) {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *shaderToken = llvm::ConstantPointerNull::get(ptrTy);
            if (inst->getOperands().size() >= 2) {
              if (auto *shaderName =
                      dynamic_cast<nir::ConstantString *>(inst->getOperand(1))) {
                if (llvm::Constant *descriptor =
                        toGraphicsShaderDescriptor(shaderName->getValue())) {
                  shaderToken =
                      llvm::ConstantExpr::getPointerCast(descriptor, ptrTy);
                }
              }
              llvm::Value *arg = toLLVMValue(inst->getOperand(1));
              if (llvm::isa<llvm::ConstantPointerNull>(shaderToken) &&
                  arg != nullptr && arg->getType()->isPointerTy()) {
                shaderToken = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_valueMap[inst] =
                m_builder->CreateCall(m_graphicsMaterialCreateFn, {shaderToken});
          }
        } else if (funcName.size() >= 8 &&
                   funcName.rfind(".SetVec4") == funcName.size() - 8) {
          if (m_graphicsMaterialSetVec4Fn != nullptr &&
              inst->getOperands().size() >= 4 &&
              inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
              inst->getOperand(1)->getType()->kind == TypeKind::Class &&
              inst->getOperand(1)->getType()->className == "Material") {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *material = llvm::ConstantPointerNull::get(ptrTy);
            llvm::Value *binding = llvm::ConstantPointerNull::get(ptrTy);
            llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
              if (arg->getType()->isPointerTy()) {
                material = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
              if (arg->getType()->isPointerTy()) {
                binding = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(3))) {
              if (arg->getType()->isPointerTy()) {
                value = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_builder->CreateCall(m_graphicsMaterialSetVec4Fn,
                                  {material, binding, value});
          }
        } else if (funcName.size() >= 11 &&
                   funcName.rfind(".SetTexture") == funcName.size() - 11) {
          if (m_graphicsMaterialSetTextureFn != nullptr &&
              inst->getOperands().size() >= 4 &&
              inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
              inst->getOperand(1)->getType()->kind == TypeKind::Class &&
              inst->getOperand(1)->getType()->className == "Material") {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *material = llvm::ConstantPointerNull::get(ptrTy);
            llvm::Value *binding = llvm::ConstantPointerNull::get(ptrTy);
            llvm::Value *texture = llvm::ConstantPointerNull::get(ptrTy);
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
              if (arg->getType()->isPointerTy()) {
                material = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
              if (arg->getType()->isPointerTy()) {
                binding = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(3))) {
              if (arg->getType()->isPointerTy()) {
                texture = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_builder->CreateCall(m_graphicsMaterialSetTextureFn,
                                  {material, binding, texture});
          }
        } else if (funcName.size() >= 11 &&
                   funcName.rfind(".SetSampler") == funcName.size() - 11) {
          if (m_graphicsMaterialSetSamplerFn != nullptr &&
              inst->getOperands().size() >= 4 &&
              inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
              inst->getOperand(1)->getType()->kind == TypeKind::Class &&
              inst->getOperand(1)->getType()->className == "Material") {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *material = llvm::ConstantPointerNull::get(ptrTy);
            llvm::Value *binding = llvm::ConstantPointerNull::get(ptrTy);
            llvm::Value *sampler = llvm::ConstantPointerNull::get(ptrTy);
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
              if (arg->getType()->isPointerTy()) {
                material = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
              if (arg->getType()->isPointerTy()) {
                binding = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(3))) {
              if (arg->getType()->isPointerTy()) {
                sampler = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_builder->CreateCall(m_graphicsMaterialSetSamplerFn,
                                  {material, binding, sampler});
          }
        } else if (funcName.size() >= 11 &&
                   funcName.rfind(".SetMatrix4") == funcName.size() - 11) {
          if (m_graphicsMaterialSetMatrix4Fn != nullptr &&
              inst->getOperands().size() >= 4 &&
              inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
              inst->getOperand(1)->getType()->kind == TypeKind::Class &&
              inst->getOperand(1)->getType()->className == "Material") {
            auto *ptrTy = llvm::PointerType::get(*m_context, 0);
            llvm::Value *material = llvm::ConstantPointerNull::get(ptrTy);
            llvm::Value *binding = llvm::ConstantPointerNull::get(ptrTy);
            llvm::Value *matrix = llvm::ConstantPointerNull::get(ptrTy);
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
              if (arg->getType()->isPointerTy()) {
                material = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
              if (arg->getType()->isPointerTy()) {
                binding = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(3))) {
              if (arg->getType()->isPointerTy()) {
                matrix = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_builder->CreateCall(m_graphicsMaterialSetMatrix4Fn,
                                  {material, binding, matrix});
          }
        } else if (funcName == "Color" && m_graphicsColorCreateFn) {
          if (inst->getOperands().size() >= 5) {
            std::vector<llvm::Value *> rgba;
            rgba.reserve(4);
            for (std::size_t i = 1; i < 5; ++i) {
              llvm::Value *value = toLLVMValue(inst->getOperand(i));
              if (value == nullptr) {
                rgba.clear();
                break;
              }
              if (!value->getType()->isDoubleTy()) {
                if (value->getType()->isFloatTy()) {
                  value = m_builder->CreateFPExt(
                      value, llvm::Type::getDoubleTy(*m_context));
                } else if (value->getType()->isIntegerTy()) {
                  value = m_builder->CreateSIToFP(
                      value, llvm::Type::getDoubleTy(*m_context));
                }
              }
              rgba.push_back(value);
            }
            if (rgba.size() == 4) {
              m_valueMap[inst] = m_builder->CreateCall(m_graphicsColorCreateFn,
                                                       rgba, inst->getName());
            }
          }
        } else if (funcName == "Vector2" && m_graphicsVector2CreateFn) {
          if (inst->getOperands().size() >= 3) {
            std::vector<llvm::Value *> values;
            values.reserve(2);
            for (std::size_t i = 1; i < 3; ++i) {
              llvm::Value *value = toLLVMValue(inst->getOperand(i));
              if (value == nullptr) {
                values.clear();
                break;
              }
              if (!value->getType()->isDoubleTy()) {
                if (value->getType()->isFloatTy()) {
                  value = m_builder->CreateFPExt(
                      value, llvm::Type::getDoubleTy(*m_context));
                } else if (value->getType()->isIntegerTy()) {
                  value = m_builder->CreateSIToFP(
                      value, llvm::Type::getDoubleTy(*m_context));
                }
              }
              values.push_back(value);
            }
            if (values.size() == 2) {
              m_valueMap[inst] = m_builder->CreateCall(
                  m_graphicsVector2CreateFn, values, inst->getName());
            }
          }
        } else if (funcName == "Vector3" && m_graphicsVector3CreateFn) {
          if (inst->getOperands().size() >= 4) {
            std::vector<llvm::Value *> values;
            values.reserve(3);
            for (std::size_t i = 1; i < 4; ++i) {
              llvm::Value *value = toLLVMValue(inst->getOperand(i));
              if (value == nullptr) {
                values.clear();
                break;
              }
              if (!value->getType()->isDoubleTy()) {
                if (value->getType()->isFloatTy()) {
                  value = m_builder->CreateFPExt(
                      value, llvm::Type::getDoubleTy(*m_context));
                } else if (value->getType()->isIntegerTy()) {
                  value = m_builder->CreateSIToFP(
                      value, llvm::Type::getDoubleTy(*m_context));
                }
              }
              values.push_back(value);
            }
            if (values.size() == 3) {
              m_valueMap[inst] = m_builder->CreateCall(
                  m_graphicsVector3CreateFn, values, inst->getName());
            }
          }
        } else if (funcName == "Vector4" && m_graphicsVector4CreateFn) {
          if (inst->getOperands().size() >= 5) {
            std::vector<llvm::Value *> values;
            values.reserve(4);
            for (std::size_t i = 1; i < 5; ++i) {
              llvm::Value *value = toLLVMValue(inst->getOperand(i));
              if (value == nullptr) {
                values.clear();
                break;
              }
              if (!value->getType()->isDoubleTy()) {
                if (value->getType()->isFloatTy()) {
                  value = m_builder->CreateFPExt(
                      value, llvm::Type::getDoubleTy(*m_context));
                } else if (value->getType()->isIntegerTy()) {
                  value = m_builder->CreateSIToFP(
                      value, llvm::Type::getDoubleTy(*m_context));
                }
              }
              values.push_back(value);
            }
            if (values.size() == 4) {
              m_valueMap[inst] = m_builder->CreateCall(
                  m_graphicsVector4CreateFn, values, inst->getName());
            }
          }
        } else if (funcName == "Scene.Create" && m_graphicsSceneCreateFn) {
          m_valueMap[inst] =
              m_builder->CreateCall(m_graphicsSceneCreateFn, {}, inst->getName());
        } else if (funcName == "Renderer2D.Create" &&
                   m_graphicsRenderer2DCreateFn) {
          m_valueMap[inst] = m_builder->CreateCall(m_graphicsRenderer2DCreateFn,
                                                   {}, inst->getName());
        } else if ((funcName == "Texture2D.Load" || funcName == "Texture.Load") &&
                   m_graphicsTextureLoadFn) {
          if (inst->getOperands().size() > 1) {
            if (llvm::Value *p = toLLVMValue(inst->getOperand(1)))
              m_valueMap[inst] = m_builder->CreateCall(m_graphicsTextureLoadFn,
                                                       {p}, inst->getName());
          }
        } else if (funcName == "Font.Load" && m_graphicsFontLoadFn) {
          if (inst->getOperands().size() > 1) {
            if (llvm::Value *p = toLLVMValue(inst->getOperand(1))) {
              m_valueMap[inst] =
                  m_builder->CreateCall(m_graphicsFontLoadFn, {p}, inst->getName());
            }
          }
        } else if (funcName == "Sampler.Create" && m_graphicsSamplerCreateFn) {
          m_valueMap[inst] = m_builder->CreateCall(m_graphicsSamplerCreateFn, {},
                                                   inst->getName());
        } else if (funcName == "Mesh.Load" && m_graphicsMeshLoadFn) {
          if (inst->getOperands().size() > 1) {
            if (llvm::Value *p = toLLVMValue(inst->getOperand(1)))
              m_valueMap[inst] = m_builder->CreateCall(m_graphicsMeshLoadFn,
                                                       {p}, inst->getName());
          }
        } else if ((funcName == "cmd.Clear" || funcName == "Graphics.Clear" ||
                    funcName == "Clear") &&
                   m_graphicsClearFn) {
          llvm::Value *color = llvm::ConstantPointerNull::get(
              llvm::PointerType::get(*m_context, 0));
          if (inst->getOperands().size() > 1 &&
              toLLVMValue(inst->getOperand(1))) {
            color = toLLVMValue(inst->getOperand(1));
          }
          m_builder->CreateCall(m_graphicsClearFn, {color});
        } else if ((funcName == "cmd.Draw" || funcName == "cmd.DrawIndexed" ||
                    funcName == "cmd.DrawInstanced" ||
                    funcName == "Graphics.Draw" || funcName == "Draw") &&
                   (m_graphicsDrawFn || m_graphicsDrawIndexedFn ||
                    m_graphicsDrawInstancedFn)) {
          llvm::Function *drawFn = m_graphicsDrawFn;
          if (funcName == "cmd.DrawIndexed") {
            drawFn = m_graphicsDrawIndexedFn;
          } else if (funcName == "cmd.DrawInstanced") {
            drawFn = m_graphicsDrawInstancedFn;
          }
          llvm::Value *target = llvm::ConstantPointerNull::get(
              llvm::PointerType::get(*m_context, 0));
          llvm::Value *shader = llvm::ConstantPointerNull::get(
              llvm::PointerType::get(*m_context, 0));
          if (inst->getOperands().size() > 1 &&
              toLLVMValue(inst->getOperand(1))) {
            target = toLLVMValue(inst->getOperand(1));
          }
          if (inst->getOperands().size() > 2 &&
              toLLVMValue(inst->getOperand(2))) {
            shader = toLLVMValue(inst->getOperand(2));
          }
          if (drawFn != nullptr) {
            if (funcName == "cmd.DrawInstanced") {
              llvm::Value *instances = llvm::ConstantInt::get(
                  llvm::Type::getInt32Ty(*m_context), 1);
              if (inst->getOperands().size() > 3 &&
                  toLLVMValue(inst->getOperand(3))) {
                instances = toLLVMValue(inst->getOperand(3));
                if (!instances->getType()->isIntegerTy(32)) {
                  instances = m_builder->CreateIntCast(
                      instances, llvm::Type::getInt32Ty(*m_context), true);
                }
              }
              m_builder->CreateCall(drawFn, {target, shader, instances});
            } else {
              m_builder->CreateCall(drawFn, {target, shader});
            }
          }
        } else if ((funcName == "Graphics.Present" || funcName == "Present") &&
                   m_graphicsPresentFn) {
          m_builder->CreateCall(m_graphicsPresentFn, {});
        } else if (funcName == "Graphics.WindowGetWidth" &&
                   m_graphicsWindowGetWidthFn) {
          if (inst->getOperands().size() > 1) {
            if (llvm::Value *w = toLLVMValue(inst->getOperand(1)))
              m_valueMap[inst] = m_builder->CreateCall(
                  m_graphicsWindowGetWidthFn, {w}, inst->getName());
          }
        } else if (funcName == "Graphics.WindowGetHeight" &&
                   m_graphicsWindowGetHeightFn) {
          if (inst->getOperands().size() > 1) {
            if (llvm::Value *w = toLLVMValue(inst->getOperand(1)))
              m_valueMap[inst] = m_builder->CreateCall(
                  m_graphicsWindowGetHeightFn, {w}, inst->getName());
          }
        } else if (funcName == "Graphics.GetLastError" &&
                   m_graphicsLastErrorFn) {
          m_valueMap[inst] =
              m_builder->CreateCall(m_graphicsLastErrorFn, {}, inst->getName());
        } else if (funcName.size() >= 13 &&
                   funcName.rfind(".CreateEntity") == funcName.size() - 13 &&
                   m_graphicsSceneCreateEntityFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *scene = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *name = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              scene = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              name = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_valueMap[inst] = m_builder->CreateCall(
              m_graphicsSceneCreateEntityFn, {scene, name}, inst->getName());
        } else if (funcName.size() >= 14 &&
                   funcName.rfind(".DestroyEntity") == funcName.size() - 14 &&
                   m_graphicsSceneDestroyEntityFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *scene = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *entity = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              scene = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              entity = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsSceneDestroyEntityFn, {scene, entity});
        } else if (funcName.size() >= 11 &&
                   funcName.rfind(".FindEntity") == funcName.size() - 11 &&
                   m_graphicsSceneFindEntityFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *scene = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *name = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              scene = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              name = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_valueMap[inst] = m_builder->CreateCall(
              m_graphicsSceneFindEntityFn, {scene, name}, inst->getName());
        } else if (funcName.size() >= 13 &&
                   funcName.rfind(".GetTransform") == funcName.size() - 13 &&
                   m_graphicsEntityGetTransformFn &&
                   inst->getOperands().size() >= 2) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *entity = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              entity = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_valueMap[inst] = m_builder->CreateCall(m_graphicsEntityGetTransformFn,
                                                   {entity}, inst->getName());
        } else if (funcName.size() >= 12 &&
                   funcName.rfind(".AddCamera2D") == funcName.size() - 12 &&
                   m_graphicsEntityAddCamera2DFn &&
                   inst->getOperands().size() >= 2) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *entity = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              entity = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_valueMap[inst] = m_builder->CreateCall(m_graphicsEntityAddCamera2DFn,
                                                   {entity}, inst->getName());
        } else if (funcName.size() >= 20 &&
                   funcName.rfind(".AddSpriteRenderer2D") ==
                       funcName.size() - 20 &&
                   m_graphicsEntityAddSpriteRenderer2DFn &&
                   inst->getOperands().size() >= 2) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *entity = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              entity = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_valueMap[inst] = m_builder->CreateCall(
              m_graphicsEntityAddSpriteRenderer2DFn, {entity}, inst->getName());
        } else if (funcName.size() >= 19 &&
                   funcName.rfind(".AddShapeRenderer2D") ==
                       funcName.size() - 19 &&
                   m_graphicsEntityAddShapeRenderer2DFn &&
                   inst->getOperands().size() >= 2) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *entity = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              entity = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_valueMap[inst] = m_builder->CreateCall(
              m_graphicsEntityAddShapeRenderer2DFn, {entity}, inst->getName());
        } else if (funcName.size() >= 18 &&
                   funcName.rfind(".AddTextRenderer2D") ==
                       funcName.size() - 18 &&
                   m_graphicsEntityAddTextRenderer2DFn &&
                   inst->getOperands().size() >= 2) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *entity = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              entity = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_valueMap[inst] = m_builder->CreateCall(
              m_graphicsEntityAddTextRenderer2DFn, {entity}, inst->getName());
        } else if (funcName.size() >= 10 &&
                   funcName.rfind(".SetParent") == funcName.size() - 10 &&
                   m_graphicsTransformSetParentFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *transform = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *parent = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              transform = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              parent = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsTransformSetParentFn,
                                {transform, parent});
        } else if (funcName.size() >= 12 &&
                   funcName.rfind(".SetPosition") == funcName.size() - 12 &&
                   m_graphicsTransformSetPositionFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *transform = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              transform = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              value = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsTransformSetPositionFn,
                                {transform, value});
        } else if (funcName.size() >= 12 &&
                   funcName.rfind(".SetRotation") == funcName.size() - 12 &&
                   m_graphicsTransformSetRotationFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *transform = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              transform = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              value = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsTransformSetRotationFn,
                                {transform, value});
        } else if (funcName.size() >= 9 &&
                   funcName.rfind(".SetScale") == funcName.size() - 9 &&
                   m_graphicsTransformSetScaleFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *transform = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              transform = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              value = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsTransformSetScaleFn, {transform, value});
        } else if (funcName.size() >= 14 &&
                   funcName.rfind(".SetClearColor") == funcName.size() - 14 &&
                   m_graphicsRenderer2DSetClearColorFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *renderer = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *color = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              renderer = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              color = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsRenderer2DSetClearColorFn,
                                {renderer, color});
        } else if (funcName.size() >= 10 &&
                   funcName.rfind(".SetCamera") == funcName.size() - 10 &&
                   m_graphicsRenderer2DSetCameraFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *renderer = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *camera = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              renderer = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              camera = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsRenderer2DSetCameraFn,
                                {renderer, camera});
        } else if (funcName.size() >= 7 &&
                   funcName.rfind(".Render") == funcName.size() - 7 &&
                   m_graphicsRenderer2DRenderFn &&
                   inst->getOperands().size() >= 3 &&
                   inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
                   inst->getOperand(1)->getType()->kind == TypeKind::Class &&
                   inst->getOperand(1)->getType()->className == "Renderer2D") {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *renderer = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *scene = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              renderer = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              scene = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsRenderer2DRenderFn, {renderer, scene});
        } else if (funcName.size() >= 8 &&
                   funcName.rfind(".SetZoom") == funcName.size() - 8 &&
                   m_graphicsCamera2DSetZoomFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *f64Ty = llvm::Type::getDoubleTy(*m_context);
          llvm::Value *camera = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *zoom = llvm::ConstantFP::get(f64Ty, 1.0);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              camera = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            zoom = arg;
            if (!zoom->getType()->isDoubleTy()) {
              if (zoom->getType()->isFloatTy()) {
                zoom = m_builder->CreateFPExt(zoom, f64Ty);
              } else if (zoom->getType()->isIntegerTy()) {
                zoom = m_builder->CreateSIToFP(zoom, f64Ty);
              }
            }
          }
          m_builder->CreateCall(m_graphicsCamera2DSetZoomFn, {camera, zoom});
        } else if (funcName.size() >= 11 &&
                   funcName.rfind(".SetPrimary") == funcName.size() - 11 &&
                   m_graphicsCamera2DSetPrimaryFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          llvm::Value *camera = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantInt::get(i32Ty, 0);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              camera = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            value = arg;
            if (!value->getType()->isIntegerTy(32)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i32Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i32Ty);
              }
            }
          }
          m_builder->CreateCall(m_graphicsCamera2DSetPrimaryFn, {camera, value});
        } else if (funcName.size() >= 11 &&
                   funcName.rfind(".SetTexture") == funcName.size() - 11 &&
                   m_graphicsSpriteRenderer2DSetTextureFn &&
                   inst->getOperands().size() >= 3 &&
                   inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
                   inst->getOperand(1)->getType()->kind == TypeKind::Class &&
                   inst->getOperand(1)->getType()->className ==
                       "SpriteRenderer2D") {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *texture = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              texture = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsSpriteRenderer2DSetTextureFn,
                                {component, texture});
        } else if (funcName.size() >= 9 &&
                   funcName.rfind(".SetColor") == funcName.size() - 9 &&
                   inst->getOperands().size() >= 3 &&
                   inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
                   inst->getOperand(1)->getType()->kind == TypeKind::Class &&
                   inst->getOperand(1)->getType()->className ==
                       "SpriteRenderer2D" &&
                   m_graphicsSpriteRenderer2DSetColorFn) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *color = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              color = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsSpriteRenderer2DSetColorFn,
                                {component, color});
        } else if (funcName.size() >= 8 &&
                   funcName.rfind(".SetSize") == funcName.size() - 8 &&
                   m_graphicsSpriteRenderer2DSetSizeFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *size = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              size = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsSpriteRenderer2DSetSizeFn,
                                {component, size});
        } else if (funcName.size() >= 9 &&
                   funcName.rfind(".SetPivot") == funcName.size() - 9 &&
                   m_graphicsSpriteRenderer2DSetPivotFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *pivot = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              pivot = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsSpriteRenderer2DSetPivotFn,
                                {component, pivot});
        } else if (funcName.size() >= 9 &&
                   funcName.rfind(".SetFlipX") == funcName.size() - 9 &&
                   m_graphicsSpriteRenderer2DSetFlipXFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantInt::get(i32Ty, 0);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            value = arg;
            if (!value->getType()->isIntegerTy(32)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i32Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i32Ty);
              }
            }
          }
          m_builder->CreateCall(m_graphicsSpriteRenderer2DSetFlipXFn,
                                {component, value});
        } else if (funcName.size() >= 9 &&
                   funcName.rfind(".SetFlipY") == funcName.size() - 9 &&
                   m_graphicsSpriteRenderer2DSetFlipYFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantInt::get(i32Ty, 0);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            value = arg;
            if (!value->getType()->isIntegerTy(32)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i32Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i32Ty);
              }
            }
          }
          m_builder->CreateCall(m_graphicsSpriteRenderer2DSetFlipYFn,
                                {component, value});
        } else if (funcName.size() >= 16 &&
                   funcName.rfind(".SetSortingLayer") == funcName.size() - 16 &&
                   inst->getOperands().size() >= 3 &&
                   inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
                   inst->getOperand(1)->getType()->kind == TypeKind::Class) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantInt::get(i32Ty, 0);
          llvm::Function *setFn = nullptr;
          const std::string &receiverClass =
              inst->getOperand(1)->getType()->className;
          if (receiverClass == "SpriteRenderer2D") {
            setFn = m_graphicsSpriteRenderer2DSetSortingLayerFn;
          } else if (receiverClass == "ShapeRenderer2D") {
            setFn = m_graphicsShapeRenderer2DSetSortingLayerFn;
          } else if (receiverClass == "TextRenderer2D") {
            setFn = m_graphicsTextRenderer2DSetSortingLayerFn;
          }
          if (setFn != nullptr) {
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
              if (arg->getType()->isPointerTy()) {
                component = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
              value = arg;
              if (!value->getType()->isIntegerTy(32)) {
                if (value->getType()->isIntegerTy()) {
                  value = m_builder->CreateIntCast(value, i32Ty, true);
                } else if (value->getType()->isFloatingPointTy()) {
                  value = m_builder->CreateFPToSI(value, i32Ty);
                }
              }
            }
            m_builder->CreateCall(setFn, {component, value});
          }
        } else if (funcName.size() >= 16 &&
                   funcName.rfind(".SetOrderInLayer") == funcName.size() - 16 &&
                   inst->getOperands().size() >= 3 &&
                   inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
                   inst->getOperand(1)->getType()->kind == TypeKind::Class) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantInt::get(i32Ty, 0);
          llvm::Function *setFn = nullptr;
          const std::string &receiverClass =
              inst->getOperand(1)->getType()->className;
          if (receiverClass == "SpriteRenderer2D") {
            setFn = m_graphicsSpriteRenderer2DSetOrderInLayerFn;
          } else if (receiverClass == "ShapeRenderer2D") {
            setFn = m_graphicsShapeRenderer2DSetOrderInLayerFn;
          } else if (receiverClass == "TextRenderer2D") {
            setFn = m_graphicsTextRenderer2DSetOrderInLayerFn;
          }
          if (setFn != nullptr) {
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
              if (arg->getType()->isPointerTy()) {
                component = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
              value = arg;
              if (!value->getType()->isIntegerTy(32)) {
                if (value->getType()->isIntegerTy()) {
                  value = m_builder->CreateIntCast(value, i32Ty, true);
                } else if (value->getType()->isFloatingPointTy()) {
                  value = m_builder->CreateFPToSI(value, i32Ty);
                }
              }
            }
            m_builder->CreateCall(setFn, {component, value});
          }
        } else if (funcName.size() >= 13 &&
                   funcName.rfind(".SetRectangle") == funcName.size() - 13 &&
                   m_graphicsShapeRenderer2DSetRectangleFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *size = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              size = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsShapeRenderer2DSetRectangleFn,
                                {component, size});
        } else if (funcName.size() >= 10 &&
                   funcName.rfind(".SetCircle") == funcName.size() - 10 &&
                   m_graphicsShapeRenderer2DSetCircleFn &&
                   inst->getOperands().size() >= 4) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *f64Ty = llvm::Type::getDoubleTy(*m_context);
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *radius = llvm::ConstantFP::get(f64Ty, 0.0);
          llvm::Value *segments = llvm::ConstantInt::get(i32Ty, 16);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            radius = arg;
            if (!radius->getType()->isDoubleTy()) {
              if (radius->getType()->isFloatTy()) {
                radius = m_builder->CreateFPExt(radius, f64Ty);
              } else if (radius->getType()->isIntegerTy()) {
                radius = m_builder->CreateSIToFP(radius, f64Ty);
              }
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(3))) {
            segments = arg;
            if (!segments->getType()->isIntegerTy(32)) {
              if (segments->getType()->isIntegerTy()) {
                segments = m_builder->CreateIntCast(segments, i32Ty, true);
              } else if (segments->getType()->isFloatingPointTy()) {
                segments = m_builder->CreateFPToSI(segments, i32Ty);
              }
            }
          }
          m_builder->CreateCall(m_graphicsShapeRenderer2DSetCircleFn,
                                {component, radius, segments});
        } else if (funcName.size() >= 8 &&
                   funcName.rfind(".SetLine") == funcName.size() - 8 &&
                   m_graphicsShapeRenderer2DSetLineFn &&
                   inst->getOperands().size() >= 5) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *f64Ty = llvm::Type::getDoubleTy(*m_context);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *start = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *end = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *thickness = llvm::ConstantFP::get(f64Ty, 1.0);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              start = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(3))) {
            if (arg->getType()->isPointerTy()) {
              end = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(4))) {
            thickness = arg;
            if (!thickness->getType()->isDoubleTy()) {
              if (thickness->getType()->isFloatTy()) {
                thickness = m_builder->CreateFPExt(thickness, f64Ty);
              } else if (thickness->getType()->isIntegerTy()) {
                thickness = m_builder->CreateSIToFP(thickness, f64Ty);
              }
            }
          }
          m_builder->CreateCall(m_graphicsShapeRenderer2DSetLineFn,
                                {component, start, end, thickness});
        } else if (funcName.size() >= 9 &&
                   funcName.rfind(".SetColor") == funcName.size() - 9 &&
                   inst->getOperands().size() >= 3 &&
                   inst->getOperand(1) != nullptr && inst->getOperand(1)->getType() != nullptr &&
                   inst->getOperand(1)->getType()->kind == TypeKind::Class) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *color = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Function *setFn = nullptr;
          const std::string &receiverClass =
              inst->getOperand(1)->getType()->className;
          if (receiverClass == "ShapeRenderer2D") {
            setFn = m_graphicsShapeRenderer2DSetColorFn;
          } else if (receiverClass == "TextRenderer2D") {
            setFn = m_graphicsTextRenderer2DSetColorFn;
          }
          if (setFn != nullptr) {
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
              if (arg->getType()->isPointerTy()) {
                component = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
              if (arg->getType()->isPointerTy()) {
                color = m_builder->CreatePointerCast(arg, ptrTy);
              }
            }
            m_builder->CreateCall(setFn, {component, color});
          }
        } else if (funcName.size() >= 10 &&
                   funcName.rfind(".SetFilled") == funcName.size() - 10 &&
                   m_graphicsShapeRenderer2DSetFilledFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantInt::get(i32Ty, 0);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            value = arg;
            if (!value->getType()->isIntegerTy(32)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i32Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i32Ty);
              }
            }
          }
          m_builder->CreateCall(m_graphicsShapeRenderer2DSetFilledFn,
                                {component, value});
        } else if (funcName.size() >= 8 &&
                   funcName.rfind(".SetFont") == funcName.size() - 8 &&
                   m_graphicsTextRenderer2DSetFontFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *font = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              font = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsTextRenderer2DSetFontFn,
                                {component, font});
        } else if (funcName.size() >= 8 &&
                   funcName.rfind(".SetText") == funcName.size() - 8 &&
                   m_graphicsTextRenderer2DSetTextFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *text = llvm::ConstantPointerNull::get(ptrTy);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            if (arg->getType()->isPointerTy()) {
              text = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          m_builder->CreateCall(m_graphicsTextRenderer2DSetTextFn,
                                {component, text});
        } else if (funcName.size() >= 12 &&
                   funcName.rfind(".SetFontSize") == funcName.size() - 12 &&
                   m_graphicsTextRenderer2DSetFontSizeFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *f64Ty = llvm::Type::getDoubleTy(*m_context);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantFP::get(f64Ty, 16.0);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            value = arg;
            if (!value->getType()->isDoubleTy()) {
              if (value->getType()->isFloatTy()) {
                value = m_builder->CreateFPExt(value, f64Ty);
              } else if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateSIToFP(value, f64Ty);
              }
            }
          }
          m_builder->CreateCall(m_graphicsTextRenderer2DSetFontSizeFn,
                                {component, value});
        } else if (funcName.size() >= 13 &&
                   funcName.rfind(".SetAlignment") == funcName.size() - 13 &&
                   m_graphicsTextRenderer2DSetAlignmentFn &&
                   inst->getOperands().size() >= 3) {
          auto *ptrTy = llvm::PointerType::get(*m_context, 0);
          auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
          llvm::Value *component = llvm::ConstantPointerNull::get(ptrTy);
          llvm::Value *value = llvm::ConstantInt::get(i32Ty, 0);
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(1))) {
            if (arg->getType()->isPointerTy()) {
              component = m_builder->CreatePointerCast(arg, ptrTy);
            }
          }
          if (llvm::Value *arg = toLLVMValue(inst->getOperand(2))) {
            value = arg;
            if (!value->getType()->isIntegerTy(32)) {
              if (value->getType()->isIntegerTy()) {
                value = m_builder->CreateIntCast(value, i32Ty, true);
              } else if (value->getType()->isFloatingPointTy()) {
                value = m_builder->CreateFPToSI(value, i32Ty);
              }
            }
          }
          m_builder->CreateCall(m_graphicsTextRenderer2DSetAlignmentFn,
                                {component, value});
        } else if (m_functionMap.count(funcName)) {
          // User-defined function call
          llvm::Function *calledFn = m_functionMap[funcName];
          std::vector<llvm::Value *> args;
          for (size_t i = 1; i < inst->getOperands().size(); i++) {
            llvm::Value *argVal = toLLVMValue(inst->getOperand(i));
            if (argVal) {
              // Type cast if needed
              llvm::Type *paramType =
                  calledFn->getFunctionType()->getParamType(i - 1);
              if (argVal->getType() != paramType) {
                if (argVal->getType()->isIntegerTy() &&
                    paramType->isIntegerTy()) {
                  argVal = m_builder->CreateIntCast(argVal, paramType, true);
                }
              }
              args.push_back(argVal);
            }
          }
          auto *result = m_builder->CreateCall(
              calledFn, args,
              calledFn->getReturnType()->isVoidTy() ? "" : inst->getName());
          m_valueMap[inst] = result;
        }
      }
    }
    break;
  }
  case nir::InstKind::FieldAccess: {
    llvm::Value *obj = toLLVMValue(inst->getOperand(0));
    llvm::Value *idxVal = toLLVMValue(inst->getOperand(1));
    if (obj && idxVal) {
      llvm::Type *structType = nullptr;
      auto nirType = inst->getOperand(0)->getType();
      if (nirType->kind == TypeKind::Pointer) {
        structType = toLLVMType(nirType->pointeeType);
      } else {
        structType = toLLVMType(nirType);
      }

      std::vector<llvm::Value *> indices = {
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), 0),
          m_builder->CreateIntCast(idxVal, llvm::Type::getInt32Ty(*m_context),
                                   false)};

      auto *gep =
          m_builder->CreateGEP(structType, obj, indices, inst->getName());
      m_valueMap[inst] = gep;
    }
    break;
  }
  case nir::InstKind::Ret: {
    llvm::Function *currentFn = m_builder->GetInsertBlock()
                                    ? m_builder->GetInsertBlock()->getParent()
                                    : nullptr;
    llvm::Type *retType = currentFn ? currentFn->getReturnType()
                                    : llvm::Type::getVoidTy(*m_context);
    if (retType->isVoidTy()) {
      m_builder->CreateRetVoid();
      break;
    }

    llvm::Value *retVal = inst->getOperands().empty()
                              ? nullptr
                              : toLLVMValue(inst->getOperand(0));
    if (retVal != nullptr && retVal->getType() != retType) {
      if (retVal->getType()->isIntegerTy() && retType->isIntegerTy()) {
        retVal = m_builder->CreateIntCast(retVal, retType, true);
      } else if (retVal->getType()->isFloatingPointTy() &&
                 retType->isFloatingPointTy()) {
        if (retVal->getType()->getPrimitiveSizeInBits() <
            retType->getPrimitiveSizeInBits()) {
          retVal = m_builder->CreateFPExt(retVal, retType);
        } else {
          retVal = m_builder->CreateFPTrunc(retVal, retType);
        }
      } else {
        retVal = nullptr;
      }
    }

    if (retVal == nullptr) {
      retVal = defaultValueForType(retType);
    }
    m_builder->CreateRet(retVal);
    break;
  }
  case nir::InstKind::GpuScopeBegin: {
    if (m_gpuScopeBeginExFn != nullptr && inst->getOperands().size() >= 2) {
      llvm::Value *modeValue = toLLVMValue(inst->getOperand(0));
      llvm::Value *targetValue = toLLVMValue(inst->getOperand(1));
      if (modeValue != nullptr && targetValue != nullptr) {
        if (modeValue->getType() != llvm::Type::getInt32Ty(*m_context)) {
          modeValue = m_builder->CreateIntCast(
              modeValue, llvm::Type::getInt32Ty(*m_context), true);
        }
        if (targetValue->getType() != llvm::Type::getInt32Ty(*m_context)) {
          targetValue = m_builder->CreateIntCast(
              targetValue, llvm::Type::getInt32Ty(*m_context), true);
        }
        (void)m_builder->CreateCall(m_gpuScopeBeginExFn,
                                    {modeValue, targetValue});
        break;
      }
    }
    if (m_gpuScopeBeginFn != nullptr) {
      (void)m_builder->CreateCall(m_gpuScopeBeginFn, {});
    }
    break;
  }
  case nir::InstKind::GpuScopeEnd: {
    if (m_gpuScopeEndFn != nullptr) {
      (void)m_builder->CreateCall(m_gpuScopeEndFn, {});
    }
    break;
  }
  case nir::InstKind::Br: {
    // Unconditional branch: operand 0 = BlockRef to target
    if (inst->getOperands().size() >= 1) {
      if (inst->getOperand(0)->getValueKind() == nir::ValueKind::Block) {
        auto *bref = static_cast<nir::BlockRef *>(inst->getOperand(0));
        nir::Block *targetBlock = bref->getBlock();
        if (targetBlock && m_blockMap.count(targetBlock->getName())) {
          m_builder->CreateBr(m_blockMap[targetBlock->getName()]);
        }
      }
    }
    break;
  }
  case nir::InstKind::CondBr: {
    // Conditional branch: operand 0 = condition, operand 1 = then BlockRef,
    // operand 2 = else BlockRef
    if (inst->getOperands().size() >= 3) {
      llvm::Value *cond = toLLVMValue(inst->getOperand(0));
      if (cond) {
        // Ensure condition is i1
        if (!cond->getType()->isIntegerTy(1)) {
          cond = m_builder->CreateICmpNE(
              cond, llvm::ConstantInt::get(cond->getType(), 0), "tobool");
        }

        auto *thenRef = static_cast<nir::BlockRef *>(inst->getOperand(1));
        auto *elseRef = static_cast<nir::BlockRef *>(inst->getOperand(2));
        llvm::BasicBlock *thenBB = m_blockMap[thenRef->getBlock()->getName()];
        llvm::BasicBlock *elseBB = m_blockMap[elseRef->getBlock()->getName()];
        if (thenBB && elseBB) {
          m_builder->CreateCondBr(cond, thenBB, elseBB);
        }
      }
    }
    break;
  }
  case nir::InstKind::TensorAdd: {
    if (inst->getOperands().size() >= 2) {
      llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
      llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
      if (lhs && rhs) {
        llvm::Value *hint = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(*m_context),
            executionHintToRuntimeValue(inst->getExecutionHint()));
        m_valueMap[inst] =
            m_builder->CreateCall(m_tensorAddFn, {lhs, rhs, hint});
      }
    }
    break;
  }
  case nir::InstKind::TensorSub: {
    if (inst->getOperands().size() >= 2) {
      llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
      llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
      if (lhs && rhs) {
        llvm::Value *hint = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(*m_context),
            executionHintToRuntimeValue(inst->getExecutionHint()));
        m_valueMap[inst] =
            m_builder->CreateCall(m_tensorSubFn, {lhs, rhs, hint});
      }
    }
    break;
  }
  case nir::InstKind::TensorMul: {
    if (inst->getOperands().size() >= 2) {
      llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
      llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
      if (lhs && rhs) {
        llvm::Value *hint = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(*m_context),
            executionHintToRuntimeValue(inst->getExecutionHint()));
        m_valueMap[inst] =
            m_builder->CreateCall(m_tensorMulFn, {lhs, rhs, hint});
      }
    }
    break;
  }
  case nir::InstKind::TensorDiv: {
    if (inst->getOperands().size() >= 2) {
      llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
      llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
      if (lhs && rhs) {
        llvm::Value *hint = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(*m_context),
            executionHintToRuntimeValue(inst->getExecutionHint()));
        m_valueMap[inst] =
            m_builder->CreateCall(m_tensorDivFn, {lhs, rhs, hint});
      }
    }
    break;
  }
  case nir::InstKind::TensorFMA: {
    if (inst->getOperands().size() >= 3) {
      llvm::Value *a = toLLVMValue(inst->getOperand(0));
      llvm::Value *b = toLLVMValue(inst->getOperand(1));
      llvm::Value *c = toLLVMValue(inst->getOperand(2));
      if (a && b && c) {
        llvm::Value *hint = llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(*m_context),
            executionHintToRuntimeValue(inst->getExecutionHint()));
        m_valueMap[inst] =
            m_builder->CreateCall(m_tensorFmaFn, {a, b, c, hint});
      }
    }
    break;
  }
  case nir::InstKind::TensorMatMul: {
    if (inst->getOperands().size() >= 2) {
      llvm::Value *lhs = toLLVMValue(inst->getOperand(0));
      llvm::Value *rhs = toLLVMValue(inst->getOperand(1));
      if (lhs && rhs) {
        auto *opaquePtrTy = llvm::PointerType::get(*m_context, 0);
        auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
        llvm::Value *nullOut = llvm::ConstantPointerNull::get(opaquePtrTy);
        llvm::Value *flags = llvm::ConstantInt::get(i32Ty, 0);
        llvm::Value *hint = llvm::ConstantInt::get(
            i32Ty, executionHintToRuntimeValue(inst->getExecutionHint()));
        m_valueMap[inst] = m_builder->CreateCall(
            m_tensorMatMulFn, {lhs, rhs, nullOut, flags, hint});
      }
    }
    break;
  }
  case nir::InstKind::TensorMatMulAdd: {
    if (inst->getOperands().size() >= 3) {
      llvm::Value *a = toLLVMValue(inst->getOperand(0));
      llvm::Value *b = toLLVMValue(inst->getOperand(1));
      llvm::Value *bias = toLLVMValue(inst->getOperand(2));
      if (a && b && bias) {
        auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
        llvm::Value *hint = llvm::ConstantInt::get(
            i32Ty, executionHintToRuntimeValue(inst->getExecutionHint()));
        m_valueMap[inst] =
            m_builder->CreateCall(m_tensorMatMulAddFn, {a, b, bias, hint});
      }
    }
    break;
  }
  case nir::InstKind::TensorLinearFused: {
    if (inst->getOperands().size() >= 4) {
      llvm::Value *a = toLLVMValue(inst->getOperand(0));
      llvm::Value *b = toLLVMValue(inst->getOperand(1));
      llvm::Value *bias = toLLVMValue(inst->getOperand(2));
      llvm::Value *residual = toLLVMValue(inst->getOperand(3));
      if (a && b && bias && residual) {
        auto *opaquePtrTy = llvm::PointerType::get(*m_context, 0);
        auto *i32Ty = llvm::Type::getInt32Ty(*m_context);
        llvm::Value *nullPacked = llvm::ConstantPointerNull::get(opaquePtrTy);
        llvm::Value *activation = llvm::ConstantInt::get(i32Ty, 0);
        llvm::Value *nullOut = llvm::ConstantPointerNull::get(opaquePtrTy);
        llvm::Value *flags = llvm::ConstantInt::get(i32Ty, 0);
        llvm::Value *hint = llvm::ConstantInt::get(
            i32Ty, executionHintToRuntimeValue(inst->getExecutionHint()));
        m_valueMap[inst] = m_builder->CreateCall(
            m_tensorLinearFusedFn, {a, b, nullPacked, bias, residual,
                                    activation, nullOut, flags, hint});
      }
    }
    break;
  }
  case nir::InstKind::TensorSlice: {
    // Slice lowering is currently a conservative no-op view.
    if (!inst->getOperands().empty()) {
      llvm::Value *base = toLLVMValue(inst->getOperand(0));
      if (base) {
        m_valueMap[inst] = base;
      }
    }
    break;
  }
  default:
    // Other ops — handled later
    break;
  }
}

void LLVMCodeGen::printIR() { m_module->print(llvm::outs(), nullptr); }

void LLVMCodeGen::writeIR(const std::string &filename) {
  std::error_code ec;
  llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
  if (ec) {
    std::cerr << "Could not open file: " << ec.message() << std::endl;
    return;
  }
  m_module->print(dest, nullptr);
}

bool LLVMCodeGen::verifyModuleIR(std::string *outError) const {
  std::string errorText;
  llvm::raw_string_ostream errorStream(errorText);
  bool broken = llvm::verifyModule(*m_module, &errorStream);
  if (broken) {
    if (outError != nullptr) {
      *outError = errorStream.str();
    }
    return false;
  }
  return true;
}

std::size_t LLVMCodeGen::instructionCount() const {
  std::size_t count = 0;
  for (const llvm::Function &func : *m_module) {
    for (const llvm::BasicBlock &bb : func) {
      count += bb.size();
    }
  }
  return count;
}

bool LLVMCodeGen::optimizeModule(const LLVMCodeGenOptions &options,
                                 std::string *outError) {
  const std::string targetTripleStr =
      options.targetTripleOverride.empty()
          ? llvm::sys::getDefaultTargetTriple()
          : options.targetTripleOverride;
  initializeTargetsForTriple(targetTripleStr);

  std::string verifyError;
  if (!verifyModuleIR(&verifyError)) {
    if (outError != nullptr) {
      *outError = "IR verification failed before optimization:\n" + verifyError;
    }
    return false;
  }

  auto targetMachine = createTargetMachine(m_module.get(), options, outError);
  if (!targetMachine) {
    return false;
  }

  llvm::LoopAnalysisManager lam;
  llvm::FunctionAnalysisManager fam;
  llvm::CGSCCAnalysisManager cgam;
  llvm::ModuleAnalysisManager mam;

  llvm::PassBuilder passBuilder(targetMachine.get());
  passBuilder.registerModuleAnalyses(mam);
  passBuilder.registerCGSCCAnalyses(cgam);
  passBuilder.registerFunctionAnalyses(fam);
  passBuilder.registerLoopAnalyses(lam);
  passBuilder.crossRegisterProxies(lam, fam, cgam, mam);

  llvm::ModulePassManager modulePassManager;
  const llvm::OptimizationLevel optLevel = mapOptLevel(options.optLevel);
  modulePassManager = passBuilder.buildPerModuleDefaultPipeline(optLevel);

  if (options.optLevel == LLVMOptLevel::Aggressive) {
    for (llvm::Function &fn : *m_module) {
      if (fn.isDeclaration()) {
        continue;
      }
      fn.addFnAttr("unsafe-fp-math", "true");
      fn.addFnAttr("no-infs-fp-math", "true");
      fn.addFnAttr("no-nans-fp-math", "true");
      fn.addFnAttr("no-signed-zeros-fp-math", "true");
      fn.addFnAttr("approx-func-fp-math", "true");
    }
    modulePassManager.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::InstCombinePass()));
    modulePassManager.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::SimplifyCFGPass()));
    modulePassManager.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::GVNPass()));
    modulePassManager.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::ADCEPass()));
    modulePassManager.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::SimplifyCFGPass()));
  }

  modulePassManager.run(*m_module, mam);

  if (options.optLevel == LLVMOptLevel::Aggressive) {
    llvm::ModulePassManager cleanupPasses;
    cleanupPasses.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::InstCombinePass()));
    cleanupPasses.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::GVNPass()));
    cleanupPasses.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::ADCEPass()));
    cleanupPasses.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::SimplifyCFGPass()));
    cleanupPasses.addPass(
        llvm::createModuleToFunctionPassAdaptor(llvm::InstCombinePass()));
    cleanupPasses.run(*m_module, mam);
  }

  if (!verifyModuleIR(&verifyError)) {
    if (outError != nullptr) {
      *outError = "IR verification failed after optimization:\n" + verifyError;
    }
    return false;
  }
  return true;
}

bool LLVMCodeGen::compileToObject(const std::string &filename,
                                  const LLVMCodeGenOptions &options,
                                  std::string *outError) {
  const std::string targetTripleStr =
      options.targetTripleOverride.empty()
          ? llvm::sys::getDefaultTargetTriple()
          : options.targetTripleOverride;
  initializeTargetsForTriple(targetTripleStr);

  std::string verifyError;
  if (!verifyModuleIR(&verifyError)) {
    if (outError != nullptr) {
      *outError =
          "IR verification failed before object emission:\n" + verifyError;
    }
    return false;
  }

  const bool ok =
      codegen::llvm_support::compileModuleToObject(m_module.get(), filename,
                                                   options, outError);
  if (ok) {
    std::cout << "Object file written to " << filename << std::endl;
  }
  return ok;
}

OwnedLLVMModule LLVMCodeGen::takeOwnedModule() {
  OwnedLLVMModule owned;
  owned.context = std::move(m_context);
  owned.module = std::move(m_module);
  m_builder.reset();
  m_valueMap.clear();
  m_functionMap.clear();
  m_blockMap.clear();
  m_structMap.clear();
  m_classMap.clear();
  m_threadThunkMap.clear();
  return owned;
}

} // namespace neuron

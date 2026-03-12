#pragma once

#include "neuronc/nir/NIR.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace neuron {

enum class LLVMOptLevel { O0, O1, O2, O3, Oz, Aggressive };
enum class LLVMTargetCPU { Native, Generic };

struct ModuleCppCompileExport {
  std::string callTarget;
  std::string libraryPath;
  std::string symbolName;
  std::vector<std::string> parameterTypes;
  std::string returnType = "void";
};

struct LLVMCodeGenOptions {
  LLVMOptLevel optLevel = LLVMOptLevel::Aggressive;
  LLVMTargetCPU targetCPU = LLVMTargetCPU::Native;
  std::string targetTripleOverride;
  bool enableWasmSimd = false;
};

struct OwnedLLVMModule {
  std::unique_ptr<llvm::LLVMContext> context;
  std::unique_ptr<llvm::Module> module;
};

class LLVMCodeGen {
public:
  LLVMCodeGen();

  /// Lower NIR Module to LLVM IR Module
  void generate(nir::Module *nirModule);
  void setModuleCppExports(
      const std::unordered_map<std::string, ModuleCppCompileExport> &exports);
  void setEntryFunctionName(std::string entryFunctionName);
  void setGraphicsShaderOutputDirectory(std::filesystem::path outputDirectory);
  void setGraphicsShaderCacheDirectory(std::filesystem::path cacheDirectory);
  void setGraphicsShaderAllowCache(bool allowCache);

  /// Print the generated LLVM IR to stdout
  void printIR();

  /// Write the generated LLVM IR to a .ll file
  void writeIR(const std::string &filename);

  /// Run LLVM IR optimization passes on the module.
  bool optimizeModule(const LLVMCodeGenOptions &options,
                      std::string *outError = nullptr);

  /// Verify module correctness.
  bool verifyModuleIR(std::string *outError = nullptr) const;

  /// Count LLVM IR instructions currently in the module.
  std::size_t instructionCount() const;

  /// Compile the LLVM IR to a native object file
  bool compileToObject(const std::string &filename,
                       const LLVMCodeGenOptions &options,
                       std::string *outError = nullptr);

  /// Get the raw LLVM Module (for further processing)
  llvm::Module *getLLVMModule() { return m_module.get(); }
  OwnedLLVMModule takeOwnedModule();

private:
  std::unique_ptr<llvm::LLVMContext> m_context;
  std::unique_ptr<llvm::Module> m_module;
  std::unique_ptr<llvm::IRBuilder<>> m_builder;
  std::string m_entryFunctionName = "Init";

  // Map NIR Values to LLVM Values
  std::unordered_map<nir::Value *, llvm::Value *> m_valueMap;

  // Map NIR function names to LLVM Functions
  std::unordered_map<std::string, llvm::Function *> m_functionMap;

  // Map NIR block names to LLVM BasicBlocks (per-function, reset each time)
  std::unordered_map<std::string, llvm::BasicBlock *> m_blockMap;

  // Map class names to LLVM types
  std::unordered_map<std::string, llvm::StructType *> m_structMap;
  std::unordered_map<std::string, nir::Class *> m_classMap;

  // Helpers
  llvm::Type *toLLVMType(NTypePtr type);
  llvm::Value *toLLVMValue(nir::Value *nirVal);
  llvm::Constant *toGraphicsShaderDescriptor(std::string_view shaderName);

  void emitFunction(nir::Function *func);
  void emitBlock(nir::Block *block, llvm::Function *llFunc);
  void emitInstruction(nir::Instruction *inst);

  // Runtime function declarations
  void declareRuntimeFunctions();
  llvm::Function *m_runtimeStartupFn = nullptr;
  llvm::Function *m_runtimeShutdownFn = nullptr;
  llvm::Function *m_moduleInitFn = nullptr;
  llvm::Function *m_threadSubmitFn = nullptr;
  llvm::Function *m_gpuScopeBeginFn = nullptr;
  llvm::Function *m_gpuScopeBeginExFn = nullptr;
  llvm::Function *m_gpuScopeEndFn = nullptr;
  llvm::Function *m_printIntFn = nullptr;
  llvm::Function *m_printStrFn = nullptr;
  llvm::Function *m_mathSqrtFn = nullptr;
  llvm::Function *m_mathPowFn = nullptr;
  llvm::Function *m_mathAbsFn = nullptr;
  llvm::Function *m_ioWriteLineFn = nullptr;
  llvm::Function *m_ioReadIntFn = nullptr;
  llvm::Function *m_ioInputIntFn = nullptr;
  llvm::Function *m_ioInputFloatFn = nullptr;
  llvm::Function *m_ioInputDoubleFn = nullptr;
  llvm::Function *m_ioInputBoolFn = nullptr;
  llvm::Function *m_ioInputStringFn = nullptr;
  llvm::Function *m_ioInputEnumFn = nullptr;
  llvm::Function *m_timeNowFn = nullptr;
  llvm::Function *m_randomIntFn = nullptr;
  llvm::Function *m_randomFloatFn = nullptr;
  llvm::Function *m_logInfoFn = nullptr;
  llvm::Function *m_logWarningFn = nullptr;
  llvm::Function *m_logErrorFn = nullptr;
  llvm::Function *m_throwFn = nullptr;
  llvm::Function *m_lastExceptionFn = nullptr;
  llvm::Function *m_clearExceptionFn = nullptr;
  llvm::Function *m_hasExceptionFn = nullptr;
  llvm::Function *m_tensorAddFn = nullptr;
  llvm::Function *m_tensorSubFn = nullptr;
  llvm::Function *m_tensorMulFn = nullptr;
  llvm::Function *m_tensorDivFn = nullptr;
  llvm::Function *m_tensorFmaFn = nullptr;
  llvm::Function *m_tensorMatMulFn = nullptr;
  llvm::Function *m_tensorMatMulAddFn = nullptr;
  llvm::Function *m_tensorLinearFusedFn = nullptr;
  llvm::Function *m_tensorConv2DBatchNormReluFn = nullptr;
  llvm::Function *m_tensorRandom2DFn = nullptr;
  llvm::Function *m_graphicsCreateWindowFn = nullptr;
  llvm::Function *m_graphicsCreateCanvasFn = nullptr;
  llvm::Function *m_graphicsCanvasFreeFn = nullptr;
  llvm::Function *m_graphicsCanvasPumpFn = nullptr;
  llvm::Function *m_graphicsCanvasShouldCloseFn = nullptr;
  llvm::Function *m_graphicsCanvasTakeResizeFn = nullptr;
  llvm::Function *m_graphicsCanvasBeginFrameFn = nullptr;
  llvm::Function *m_graphicsCanvasEndFrameFn = nullptr;
  llvm::Function *m_graphicsMaterialCreateFn = nullptr;
  llvm::Function *m_graphicsMaterialSetVec4Fn = nullptr;
  llvm::Function *m_graphicsMaterialSetTextureFn = nullptr;
  llvm::Function *m_graphicsMaterialSetSamplerFn = nullptr;
  llvm::Function *m_graphicsMaterialSetMatrix4Fn = nullptr;
  llvm::Function *m_graphicsColorCreateFn = nullptr;
  llvm::Function *m_graphicsVector2CreateFn = nullptr;
  llvm::Function *m_graphicsVector3CreateFn = nullptr;
  llvm::Function *m_graphicsVector4CreateFn = nullptr;
  llvm::Function *m_graphicsTextureLoadFn = nullptr;
  llvm::Function *m_graphicsSamplerCreateFn = nullptr;
  llvm::Function *m_graphicsMeshLoadFn = nullptr;
  llvm::Function *m_graphicsDrawFn = nullptr;
  llvm::Function *m_graphicsDrawIndexedFn = nullptr;
  llvm::Function *m_graphicsDrawInstancedFn = nullptr;
  llvm::Function *m_graphicsClearFn = nullptr;
  llvm::Function *m_graphicsPresentFn = nullptr;
  llvm::Function *m_graphicsWindowGetWidthFn = nullptr;
  llvm::Function *m_graphicsWindowGetHeightFn = nullptr;
  llvm::Function *m_graphicsLastErrorFn = nullptr;
  llvm::Function *m_graphicsSceneCreateFn = nullptr;
  llvm::Function *m_graphicsSceneCreateEntityFn = nullptr;
  llvm::Function *m_graphicsSceneDestroyEntityFn = nullptr;
  llvm::Function *m_graphicsSceneFindEntityFn = nullptr;
  llvm::Function *m_graphicsEntityGetTransformFn = nullptr;
  llvm::Function *m_graphicsEntityAddCamera2DFn = nullptr;
  llvm::Function *m_graphicsEntityAddSpriteRenderer2DFn = nullptr;
  llvm::Function *m_graphicsEntityAddShapeRenderer2DFn = nullptr;
  llvm::Function *m_graphicsEntityAddTextRenderer2DFn = nullptr;
  llvm::Function *m_graphicsTransformSetParentFn = nullptr;
  llvm::Function *m_graphicsTransformSetPositionFn = nullptr;
  llvm::Function *m_graphicsTransformSetRotationFn = nullptr;
  llvm::Function *m_graphicsTransformSetScaleFn = nullptr;
  llvm::Function *m_graphicsRenderer2DCreateFn = nullptr;
  llvm::Function *m_graphicsRenderer2DSetClearColorFn = nullptr;
  llvm::Function *m_graphicsRenderer2DSetCameraFn = nullptr;
  llvm::Function *m_graphicsRenderer2DRenderFn = nullptr;
  llvm::Function *m_graphicsCamera2DSetZoomFn = nullptr;
  llvm::Function *m_graphicsCamera2DSetPrimaryFn = nullptr;
  llvm::Function *m_graphicsSpriteRenderer2DSetTextureFn = nullptr;
  llvm::Function *m_graphicsSpriteRenderer2DSetColorFn = nullptr;
  llvm::Function *m_graphicsSpriteRenderer2DSetSizeFn = nullptr;
  llvm::Function *m_graphicsSpriteRenderer2DSetPivotFn = nullptr;
  llvm::Function *m_graphicsSpriteRenderer2DSetFlipXFn = nullptr;
  llvm::Function *m_graphicsSpriteRenderer2DSetFlipYFn = nullptr;
  llvm::Function *m_graphicsSpriteRenderer2DSetSortingLayerFn = nullptr;
  llvm::Function *m_graphicsSpriteRenderer2DSetOrderInLayerFn = nullptr;
  llvm::Function *m_graphicsShapeRenderer2DSetRectangleFn = nullptr;
  llvm::Function *m_graphicsShapeRenderer2DSetCircleFn = nullptr;
  llvm::Function *m_graphicsShapeRenderer2DSetLineFn = nullptr;
  llvm::Function *m_graphicsShapeRenderer2DSetColorFn = nullptr;
  llvm::Function *m_graphicsShapeRenderer2DSetFilledFn = nullptr;
  llvm::Function *m_graphicsShapeRenderer2DSetSortingLayerFn = nullptr;
  llvm::Function *m_graphicsShapeRenderer2DSetOrderInLayerFn = nullptr;
  llvm::Function *m_graphicsFontLoadFn = nullptr;
  llvm::Function *m_graphicsTextRenderer2DSetFontFn = nullptr;
  llvm::Function *m_graphicsTextRenderer2DSetTextFn = nullptr;
  llvm::Function *m_graphicsTextRenderer2DSetFontSizeFn = nullptr;
  llvm::Function *m_graphicsTextRenderer2DSetColorFn = nullptr;
  llvm::Function *m_graphicsTextRenderer2DSetAlignmentFn = nullptr;
  llvm::Function *m_graphicsTextRenderer2DSetSortingLayerFn = nullptr;
  llvm::Function *m_graphicsTextRenderer2DSetOrderInLayerFn = nullptr;
  llvm::Function *m_nnSelfTestFn = nullptr;
  llvm::Function *m_moduleCppRegisterFn = nullptr;
  llvm::Function *m_moduleCppStartupFn = nullptr;
  llvm::Function *m_moduleCppShutdownFn = nullptr;
  llvm::Function *m_moduleCppInvokeFn = nullptr;
  std::unordered_map<std::string, llvm::Function *> m_threadThunkMap;
  std::unordered_map<std::string, ModuleCppCompileExport> m_moduleCppExports;
  std::unordered_map<std::string, llvm::Constant *> m_graphicsShaderDescriptorMap;
  std::unordered_map<std::string, std::vector<uint32_t>> m_graphicsSpirvCache;
  std::unordered_map<std::string, std::string> m_graphicsWgslCache;
  std::filesystem::path m_graphicsShaderOutputDirectory;
  std::filesystem::path m_graphicsShaderCacheDirectory;
  bool m_graphicsShaderAllowCache = true;
  nir::Module *m_sourceModule = nullptr;
};

} // namespace neuron

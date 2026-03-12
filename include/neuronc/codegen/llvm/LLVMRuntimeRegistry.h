#pragma once

#include "neuronc/codegen/LLVMCodeGen.h"

namespace neuron {
namespace codegen::llvm_support {

struct LLVMRuntimeRegistryState {
  llvm::LLVMContext *context = nullptr;
  llvm::Module *module = nullptr;

  llvm::Function **runtimeStartupFn = nullptr;
  llvm::Function **runtimeShutdownFn = nullptr;
  llvm::Function **moduleInitFn = nullptr;
  llvm::Function **threadSubmitFn = nullptr;
  llvm::Function **gpuScopeBeginFn = nullptr;
  llvm::Function **gpuScopeBeginExFn = nullptr;
  llvm::Function **gpuScopeEndFn = nullptr;
  llvm::Function **printIntFn = nullptr;
  llvm::Function **printStrFn = nullptr;
  llvm::Function **ioWriteLineFn = nullptr;
  llvm::Function **ioReadIntFn = nullptr;
  llvm::Function **ioInputIntFn = nullptr;
  llvm::Function **ioInputFloatFn = nullptr;
  llvm::Function **ioInputDoubleFn = nullptr;
  llvm::Function **ioInputBoolFn = nullptr;
  llvm::Function **ioInputStringFn = nullptr;
  llvm::Function **ioInputEnumFn = nullptr;
  llvm::Function **timeNowFn = nullptr;
  llvm::Function **randomIntFn = nullptr;
  llvm::Function **randomFloatFn = nullptr;
  llvm::Function **logInfoFn = nullptr;
  llvm::Function **logWarningFn = nullptr;
  llvm::Function **logErrorFn = nullptr;
  llvm::Function **throwFn = nullptr;
  llvm::Function **lastExceptionFn = nullptr;
  llvm::Function **clearExceptionFn = nullptr;
  llvm::Function **hasExceptionFn = nullptr;
  llvm::Function **tensorAddFn = nullptr;
  llvm::Function **tensorSubFn = nullptr;
  llvm::Function **tensorMulFn = nullptr;
  llvm::Function **tensorDivFn = nullptr;
  llvm::Function **tensorFmaFn = nullptr;
  llvm::Function **tensorMatMulFn = nullptr;
  llvm::Function **tensorMatMulAddFn = nullptr;
  llvm::Function **tensorLinearFusedFn = nullptr;
  llvm::Function **tensorConv2DBatchNormReluFn = nullptr;
  llvm::Function **tensorRandom2DFn = nullptr;
  llvm::Function **graphicsCreateWindowFn = nullptr;
  llvm::Function **graphicsCreateCanvasFn = nullptr;
  llvm::Function **graphicsCanvasFreeFn = nullptr;
  llvm::Function **graphicsCanvasPumpFn = nullptr;
  llvm::Function **graphicsCanvasShouldCloseFn = nullptr;
  llvm::Function **graphicsCanvasTakeResizeFn = nullptr;
  llvm::Function **graphicsCanvasBeginFrameFn = nullptr;
  llvm::Function **graphicsCanvasEndFrameFn = nullptr;
  llvm::Function **graphicsMaterialCreateFn = nullptr;
  llvm::Function **graphicsMaterialSetVec4Fn = nullptr;
  llvm::Function **graphicsMaterialSetTextureFn = nullptr;
  llvm::Function **graphicsMaterialSetSamplerFn = nullptr;
  llvm::Function **graphicsMaterialSetMatrix4Fn = nullptr;
  llvm::Function **graphicsColorCreateFn = nullptr;
  llvm::Function **graphicsVector2CreateFn = nullptr;
  llvm::Function **graphicsVector3CreateFn = nullptr;
  llvm::Function **graphicsVector4CreateFn = nullptr;
  llvm::Function **graphicsTextureLoadFn = nullptr;
  llvm::Function **graphicsSamplerCreateFn = nullptr;
  llvm::Function **graphicsMeshLoadFn = nullptr;
  llvm::Function **graphicsDrawFn = nullptr;
  llvm::Function **graphicsDrawIndexedFn = nullptr;
  llvm::Function **graphicsDrawInstancedFn = nullptr;
  llvm::Function **graphicsClearFn = nullptr;
  llvm::Function **graphicsPresentFn = nullptr;
  llvm::Function **graphicsWindowGetWidthFn = nullptr;
  llvm::Function **graphicsWindowGetHeightFn = nullptr;
  llvm::Function **graphicsLastErrorFn = nullptr;
  llvm::Function **graphicsSceneCreateFn = nullptr;
  llvm::Function **graphicsSceneCreateEntityFn = nullptr;
  llvm::Function **graphicsSceneDestroyEntityFn = nullptr;
  llvm::Function **graphicsSceneFindEntityFn = nullptr;
  llvm::Function **graphicsEntityGetTransformFn = nullptr;
  llvm::Function **graphicsEntityAddCamera2DFn = nullptr;
  llvm::Function **graphicsEntityAddSpriteRenderer2DFn = nullptr;
  llvm::Function **graphicsEntityAddShapeRenderer2DFn = nullptr;
  llvm::Function **graphicsEntityAddTextRenderer2DFn = nullptr;
  llvm::Function **graphicsTransformSetParentFn = nullptr;
  llvm::Function **graphicsTransformSetPositionFn = nullptr;
  llvm::Function **graphicsTransformSetRotationFn = nullptr;
  llvm::Function **graphicsTransformSetScaleFn = nullptr;
  llvm::Function **graphicsRenderer2DCreateFn = nullptr;
  llvm::Function **graphicsRenderer2DSetClearColorFn = nullptr;
  llvm::Function **graphicsRenderer2DSetCameraFn = nullptr;
  llvm::Function **graphicsRenderer2DRenderFn = nullptr;
  llvm::Function **graphicsCamera2DSetZoomFn = nullptr;
  llvm::Function **graphicsCamera2DSetPrimaryFn = nullptr;
  llvm::Function **graphicsSpriteRenderer2DSetTextureFn = nullptr;
  llvm::Function **graphicsSpriteRenderer2DSetColorFn = nullptr;
  llvm::Function **graphicsSpriteRenderer2DSetSizeFn = nullptr;
  llvm::Function **graphicsSpriteRenderer2DSetPivotFn = nullptr;
  llvm::Function **graphicsSpriteRenderer2DSetFlipXFn = nullptr;
  llvm::Function **graphicsSpriteRenderer2DSetFlipYFn = nullptr;
  llvm::Function **graphicsSpriteRenderer2DSetSortingLayerFn = nullptr;
  llvm::Function **graphicsSpriteRenderer2DSetOrderInLayerFn = nullptr;
  llvm::Function **graphicsShapeRenderer2DSetRectangleFn = nullptr;
  llvm::Function **graphicsShapeRenderer2DSetCircleFn = nullptr;
  llvm::Function **graphicsShapeRenderer2DSetLineFn = nullptr;
  llvm::Function **graphicsShapeRenderer2DSetColorFn = nullptr;
  llvm::Function **graphicsShapeRenderer2DSetFilledFn = nullptr;
  llvm::Function **graphicsShapeRenderer2DSetSortingLayerFn = nullptr;
  llvm::Function **graphicsShapeRenderer2DSetOrderInLayerFn = nullptr;
  llvm::Function **graphicsFontLoadFn = nullptr;
  llvm::Function **graphicsTextRenderer2DSetFontFn = nullptr;
  llvm::Function **graphicsTextRenderer2DSetTextFn = nullptr;
  llvm::Function **graphicsTextRenderer2DSetFontSizeFn = nullptr;
  llvm::Function **graphicsTextRenderer2DSetColorFn = nullptr;
  llvm::Function **graphicsTextRenderer2DSetAlignmentFn = nullptr;
  llvm::Function **graphicsTextRenderer2DSetSortingLayerFn = nullptr;
  llvm::Function **graphicsTextRenderer2DSetOrderInLayerFn = nullptr;
  llvm::Function **nnSelfTestFn = nullptr;
  llvm::Function **moduleCppRegisterFn = nullptr;
  llvm::Function **moduleCppStartupFn = nullptr;
  llvm::Function **moduleCppShutdownFn = nullptr;
  llvm::Function **moduleCppInvokeFn = nullptr;
};

void declareRuntimeFunctions(LLVMRuntimeRegistryState &state);

} // namespace codegen::llvm_support
} // namespace neuron


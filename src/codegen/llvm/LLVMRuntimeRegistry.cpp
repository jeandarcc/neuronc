#include "neuronc/codegen/llvm/LLVMRuntimeRegistry.h"

namespace neuron {
namespace codegen::llvm_support {

void declareRuntimeFunctions(LLVMRuntimeRegistryState &state) {
  auto &context = *state.context;
  auto *module = state.module;

  auto *voidType = llvm::Type::getVoidTy(context);
  auto *i32Type = llvm::Type::getInt32Ty(context);
  auto *i64Type = llvm::Type::getInt64Ty(context);
  auto *f32Type = llvm::Type::getFloatTy(context);
  auto *f64Type = llvm::Type::getDoubleTy(context);
  auto *ptrType = llvm::PointerType::get(context, 0);

  *state.runtimeStartupFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_runtime_startup", module);
  *state.runtimeShutdownFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_runtime_shutdown", module);
  *state.moduleInitFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_module_init", module);
  *state.threadSubmitFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_thread_submit", module);
  *state.gpuScopeBeginFn = llvm::Function::Create(
      llvm::FunctionType::get(i32Type, {}, false),
      llvm::Function::ExternalLinkage, "neuron_gpu_scope_begin", module);
  *state.gpuScopeBeginExFn = llvm::Function::Create(
      llvm::FunctionType::get(i32Type, {i32Type, i32Type}, false),
      llvm::Function::ExternalLinkage, "neuron_gpu_scope_begin_ex", module);
  *state.gpuScopeEndFn = llvm::Function::Create(
      llvm::FunctionType::get(i32Type, {}, false),
      llvm::Function::ExternalLinkage, "neuron_gpu_scope_end", module);

  *state.printIntFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {i64Type}, false),
      llvm::Function::ExternalLinkage, "neuron_print_int", module);
  *state.printStrFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_print_str", module);
  *state.ioWriteLineFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_io_write_line", module);
  *state.ioReadIntFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {}, false),
      llvm::Function::ExternalLinkage, "neuron_io_read_int", module);
  *state.ioInputIntFn = llvm::Function::Create(
      llvm::FunctionType::get(
          i64Type,
          {ptrType, i64Type, i64Type, i64Type, i64Type, i64Type, i64Type,
           i64Type},
          false),
      llvm::Function::ExternalLinkage, "neuron_io_input_int", module);
  *state.ioInputFloatFn = llvm::Function::Create(
      llvm::FunctionType::get(
          f32Type,
          {ptrType, i64Type, f32Type, i64Type, f32Type, i64Type, f32Type,
           i64Type},
          false),
      llvm::Function::ExternalLinkage, "neuron_io_input_float", module);
  *state.ioInputDoubleFn = llvm::Function::Create(
      llvm::FunctionType::get(
          f64Type,
          {ptrType, i64Type, f64Type, i64Type, f64Type, i64Type, f64Type,
           i64Type},
          false),
      llvm::Function::ExternalLinkage, "neuron_io_input_double", module);
  *state.ioInputBoolFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {ptrType, i64Type, i64Type, i64Type},
                              false),
      llvm::Function::ExternalLinkage, "neuron_io_input_bool", module);
  *state.ioInputStringFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType, i64Type, i64Type, ptrType, i64Type},
                              false),
      llvm::Function::ExternalLinkage, "neuron_io_input_string", module);
  *state.ioInputEnumFn = llvm::Function::Create(
      llvm::FunctionType::get(
          i64Type, {ptrType, ptrType, i64Type, i64Type, i64Type, i64Type},
          false),
      llvm::Function::ExternalLinkage, "neuron_io_input_enum", module);
  *state.timeNowFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {}, false),
      llvm::Function::ExternalLinkage, "neuron_time_now_ms", module);
  *state.randomIntFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {i64Type, i64Type}, false),
      llvm::Function::ExternalLinkage, "neuron_random_int", module);
  *state.randomFloatFn = llvm::Function::Create(
      llvm::FunctionType::get(f64Type, {}, false),
      llvm::Function::ExternalLinkage, "neuron_random_float", module);
  *state.logInfoFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_log_info", module);
  *state.logWarningFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_log_warning", module);
  *state.logErrorFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_log_error", module);
  *state.throwFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_throw", module);
  *state.lastExceptionFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_last_exception", module);
  *state.clearExceptionFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_clear_exception", module);
  *state.hasExceptionFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {}, false),
      llvm::Function::ExternalLinkage, "neuron_has_exception", module);

  auto *tensorBinaryDispatchType =
      llvm::FunctionType::get(ptrType, {ptrType, ptrType, i32Type}, false);
  *state.tensorAddFn = llvm::Function::Create(
      tensorBinaryDispatchType, llvm::Function::ExternalLinkage,
      "neuron_tensor_add_ex", module);
  *state.tensorSubFn = llvm::Function::Create(
      tensorBinaryDispatchType, llvm::Function::ExternalLinkage,
      "neuron_tensor_sub_ex", module);
  *state.tensorMulFn = llvm::Function::Create(
      tensorBinaryDispatchType, llvm::Function::ExternalLinkage,
      "neuron_tensor_mul_ex", module);
  *state.tensorDivFn = llvm::Function::Create(
      tensorBinaryDispatchType, llvm::Function::ExternalLinkage,
      "neuron_tensor_div_ex", module);
  *state.tensorFmaFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType, ptrType, ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage, "neuron_tensor_fma_ex", module);
  *state.tensorMatMulFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType, ptrType, ptrType, i32Type, i32Type}, false),
      llvm::Function::ExternalLinkage, "neuron_tensor_matmul_ex_hint", module);
  *state.tensorMatMulAddFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType, ptrType, ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage, "neuron_tensor_matmul_add_ex_hint", module);
  *state.tensorLinearFusedFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType,
                              {ptrType, ptrType, ptrType, ptrType, ptrType,
                               i32Type, ptrType, i32Type, i32Type},
                              false),
      llvm::Function::ExternalLinkage, "neuron_tensor_linear_fused_ex_hint", module);
  *state.tensorConv2DBatchNormReluFn = llvm::Function::Create(
      llvm::FunctionType::get(
          ptrType,
          {ptrType, ptrType, ptrType, ptrType, ptrType, ptrType, ptrType,
           f32Type, i32Type, i32Type, i32Type, i32Type, i32Type},
          false),
      llvm::Function::ExternalLinkage,
      "neuron_tensor_conv2d_batchnorm_relu_ex_hint", module);
  llvm::Function::Create(llvm::FunctionType::get(ptrType, {}, false),
                         llvm::Function::ExternalLinkage,
                         "neuron_tensor_create_default", module);
  *state.tensorRandom2DFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {i32Type, i32Type}, false),
      llvm::Function::ExternalLinkage, "neuron_tensor_random_2d", module);

  *state.graphicsCreateWindowFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {i32Type, i32Type, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_create_window", module);
  *state.graphicsCreateCanvasFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_create_canvas", module);
  *state.graphicsCanvasFreeFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_canvas_free", module);
  *state.graphicsCanvasPumpFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_canvas_pump", module);
  *state.graphicsCanvasShouldCloseFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_canvas_should_close", module);
  *state.graphicsCanvasTakeResizeFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_canvas_take_resize", module);
  *state.graphicsCanvasBeginFrameFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_canvas_begin_frame", module);
  *state.graphicsCanvasEndFrameFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_canvas_end_frame", module);
  *state.graphicsMaterialCreateFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_material_create", module);
  *state.graphicsMaterialSetVec4Fn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_material_set_vec4",
      module);
  *state.graphicsMaterialSetTextureFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_material_set_texture",
      module);
  *state.graphicsMaterialSetSamplerFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_material_set_sampler",
      module);
  *state.graphicsMaterialSetMatrix4Fn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_material_set_matrix4",
      module);
  *state.graphicsColorCreateFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {f64Type, f64Type, f64Type, f64Type},
                              false),
      llvm::Function::ExternalLinkage, "neuron_graphics_color_rgba", module);
  *state.graphicsVector2CreateFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {f64Type, f64Type}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_vector2_create", module);
  *state.graphicsVector3CreateFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {f64Type, f64Type, f64Type}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_vector3_create", module);
  *state.graphicsVector4CreateFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {f64Type, f64Type, f64Type, f64Type},
                              false),
      llvm::Function::ExternalLinkage, "neuron_graphics_vector4_create", module);
  *state.graphicsTextureLoadFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_texture_load", module);
  *state.graphicsSamplerCreateFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_sampler_create",
      module);
  *state.graphicsMeshLoadFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_mesh_load", module);
  *state.graphicsDrawFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_draw", module);
  *state.graphicsDrawIndexedFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_draw_indexed", module);
  *state.graphicsDrawInstancedFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_draw_instanced",
      module);
  *state.graphicsClearFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_clear", module);
  *state.graphicsPresentFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_present", module);
  *state.graphicsWindowGetWidthFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_window_get_width", module);
  *state.graphicsWindowGetHeightFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_window_get_height", module);
  *state.graphicsLastErrorFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_last_error", module);
  *state.graphicsSceneCreateFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_scene_create", module);
  *state.graphicsSceneCreateEntityFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_scene_create_entity",
      module);
  *state.graphicsSceneDestroyEntityFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_scene_destroy_entity",
      module);
  *state.graphicsSceneFindEntityFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_scene_find_entity",
      module);
  *state.graphicsEntityGetTransformFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_entity_get_transform",
      module);
  *state.graphicsEntityAddCamera2DFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_entity_add_camera2d",
      module);
  *state.graphicsEntityAddSpriteRenderer2DFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_entity_add_sprite_renderer2d", module);
  *state.graphicsEntityAddShapeRenderer2DFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_entity_add_shape_renderer2d", module);
  *state.graphicsEntityAddTextRenderer2DFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_entity_add_text_renderer2d", module);
  *state.graphicsTransformSetParentFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_transform_set_parent",
      module);
  *state.graphicsTransformSetPositionFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_transform_set_position",
      module);
  *state.graphicsTransformSetRotationFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_transform_set_rotation",
      module);
  *state.graphicsTransformSetScaleFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_transform_set_scale",
      module);
  *state.graphicsRenderer2DCreateFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_renderer2d_create",
      module);
  *state.graphicsRenderer2DSetClearColorFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_renderer2d_set_clear_color", module);
  *state.graphicsRenderer2DSetCameraFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_renderer2d_set_camera",
      module);
  *state.graphicsRenderer2DRenderFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_renderer2d_render",
      module);
  *state.graphicsCamera2DSetZoomFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, f64Type}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_camera2d_set_zoom",
      module);
  *state.graphicsCamera2DSetPrimaryFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_camera2d_set_primary",
      module);
  *state.graphicsSpriteRenderer2DSetTextureFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_sprite_renderer2d_set_texture", module);
  *state.graphicsSpriteRenderer2DSetColorFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_sprite_renderer2d_set_color", module);
  *state.graphicsSpriteRenderer2DSetSizeFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_sprite_renderer2d_set_size", module);
  *state.graphicsSpriteRenderer2DSetPivotFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_sprite_renderer2d_set_pivot", module);
  *state.graphicsSpriteRenderer2DSetFlipXFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_sprite_renderer2d_set_flip_x", module);
  *state.graphicsSpriteRenderer2DSetFlipYFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_sprite_renderer2d_set_flip_y", module);
  *state.graphicsSpriteRenderer2DSetSortingLayerFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_sprite_renderer2d_set_sorting_layer", module);
  *state.graphicsSpriteRenderer2DSetOrderInLayerFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_sprite_renderer2d_set_order_in_layer", module);
  *state.graphicsShapeRenderer2DSetRectangleFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_shape_renderer2d_set_rectangle", module);
  *state.graphicsShapeRenderer2DSetCircleFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, f64Type, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_shape_renderer2d_set_circle", module);
  *state.graphicsShapeRenderer2DSetLineFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType, ptrType, f64Type},
                              false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_shape_renderer2d_set_line", module);
  *state.graphicsShapeRenderer2DSetColorFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_shape_renderer2d_set_color", module);
  *state.graphicsShapeRenderer2DSetFilledFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_shape_renderer2d_set_filled", module);
  *state.graphicsShapeRenderer2DSetSortingLayerFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_shape_renderer2d_set_sorting_layer", module);
  *state.graphicsShapeRenderer2DSetOrderInLayerFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_shape_renderer2d_set_order_in_layer", module);
  *state.graphicsFontLoadFn = llvm::Function::Create(
      llvm::FunctionType::get(ptrType, {ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_graphics_font_load", module);
  *state.graphicsTextRenderer2DSetFontFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_text_renderer2d_set_font", module);
  *state.graphicsTextRenderer2DSetTextFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_text_renderer2d_set_text", module);
  *state.graphicsTextRenderer2DSetFontSizeFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, f64Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_text_renderer2d_set_font_size", module);
  *state.graphicsTextRenderer2DSetColorFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_text_renderer2d_set_color", module);
  *state.graphicsTextRenderer2DSetAlignmentFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_text_renderer2d_set_alignment", module);
  *state.graphicsTextRenderer2DSetSortingLayerFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_text_renderer2d_set_sorting_layer", module);
  *state.graphicsTextRenderer2DSetOrderInLayerFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {ptrType, i32Type}, false),
      llvm::Function::ExternalLinkage,
      "neuron_graphics_text_renderer2d_set_order_in_layer", module);

  *state.nnSelfTestFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {}, false),
      llvm::Function::ExternalLinkage, "neuron_nn_self_test", module);
  *state.moduleCppRegisterFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {ptrType, ptrType, ptrType, ptrType, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_modulecpp_register", module);
  *state.moduleCppStartupFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {}, false),
      llvm::Function::ExternalLinkage, "neuron_modulecpp_startup", module);
  *state.moduleCppShutdownFn = llvm::Function::Create(
      llvm::FunctionType::get(voidType, {}, false),
      llvm::Function::ExternalLinkage, "neuron_modulecpp_shutdown", module);
  *state.moduleCppInvokeFn = llvm::Function::Create(
      llvm::FunctionType::get(i64Type, {ptrType, ptrType, i64Type, ptrType}, false),
      llvm::Function::ExternalLinkage, "neuron_modulecpp_invoke", module);
}

} // namespace codegen::llvm_support
} // namespace neuron


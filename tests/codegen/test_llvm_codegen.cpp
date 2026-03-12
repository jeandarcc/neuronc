// LLVM codegen and optimizer tests - included from tests/test_main.cpp
#include "neuronc/codegen/LLVMCodeGen.h"
#include "neuronc/nir/Optimizer.h"
#include "neuronc/ncon/Sha256.h"

#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace neuron;

namespace {

std::string quoteTestShellArg(const std::string &value) {
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

void ensureShaderCacheArtifact(const std::string &glslSource, bool isVertexStage) {
  namespace fs = std::filesystem;

  const fs::path cacheDir = fs::temp_directory_path() / "npp_shader_cache";
  std::error_code ec;
  fs::create_directories(cacheDir, ec);

  const std::string key = std::to_string(
      std::hash<std::string>{}((isVertexStage ? "vert:" : "frag:") + glslSource));
  const fs::path sourcePath = cacheDir / (key + (isVertexStage ? ".vert" : ".frag"));
  const fs::path outputPath = cacheDir / (key + ".spv");
  if (fs::exists(outputPath)) {
    return;
  }

  std::ofstream out(sourcePath, std::ios::binary);
  if (!out.is_open()) {
    throw std::runtime_error("failed to write test shader source: " +
                             sourcePath.string());
  }
  out << glslSource;
  out.close();

  const std::string stage = isVertexStage ? "vert" : "frag";
  const std::string glslangCommand =
      "glslangValidator --version >nul 2>&1 && glslangValidator -V -S " + stage +
      " " + quoteTestShellArg(sourcePath.string()) + " -o " +
      quoteTestShellArg(outputPath.string()) + " >nul 2>&1";
  if (std::system(glslangCommand.c_str()) == 0 && fs::exists(outputPath)) {
    return;
  }

  const std::string glslcCommand =
      "glslc --version >nul 2>&1 && glslc -fshader-stage=" + stage + " " +
      quoteTestShellArg(sourcePath.string()) + " -o " +
      quoteTestShellArg(outputPath.string()) + " >nul 2>&1";
  if (std::system(glslcCommand.c_str()) != 0 || !fs::exists(outputPath)) {
    throw std::runtime_error("failed to precompile test shader artifact: " +
                             sourcePath.string());
  }
}

void ensureWgslCacheArtifact(const std::string &glslSource, bool isVertexStage,
                             const std::filesystem::path &cacheDir,
                             const std::string &wgslSource) {
  namespace fs = std::filesystem;

  const fs::path spirvPath = fs::temp_directory_path() / "npp_shader_cache" /
                             (std::to_string(std::hash<std::string>{}(
                                  (isVertexStage ? "vert:" : "frag:") + glslSource)) +
                              ".spv");
  std::ifstream in(spirvPath, std::ios::binary);
  if (!in.is_open()) {
    throw std::runtime_error("failed to open SPIR-V cache artifact: " +
                             spirvPath.string());
  }

  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
  if (bytes.empty()) {
    throw std::runtime_error("empty SPIR-V cache artifact: " + spirvPath.string());
  }

  std::error_code ec;
  fs::create_directories(cacheDir, ec);
  const fs::path wgslPath =
      cacheDir / (neuron::ncon::sha256Hex(bytes) + ".wgsl");
  std::ofstream out(wgslPath, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    throw std::runtime_error("failed to write WGSL cache artifact: " +
                             wgslPath.string());
  }
  out << wgslSource;
  if (!out.good()) {
    throw std::runtime_error("failed to flush WGSL cache artifact: " +
                             wgslPath.string());
  }
}

} // namespace

TEST(LLVMCodegenProducesTypeSafeFallbackReturn) {
  auto module = std::make_unique<nir::Module>("llvm_type_safe_ret");

  nir::Function *getPtr =
      module->createFunction("GetPtr", NType::makePointer(NType::makeInt()));
  getPtr->createBlock("entry");

  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *initEntry = init->createBlock("entry");
  auto initRet =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  initEntry->addInstruction(std::move(initRet));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  ASSERT_TRUE(codegen.verifyModuleIR(&verifyError));

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("define ptr @GetPtr()") != std::string::npos);
  ASSERT_TRUE(ir.find("ret ptr null") != std::string::npos);
  return true;
}

TEST(LLVMOptimizerAggressiveFlattensInitCallInMain) {
  auto module = std::make_unique<nir::Module>("llvm_inline_main");
  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");

  auto printCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeVoid(), "");
  printCall->addOperand(new nir::ConstantString("Print"));
  printCall->addOperand(new nir::ConstantString("hello"));
  entry->addInstruction(std::move(printCall));
  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());
  const std::size_t before = codegen.instructionCount();

  LLVMCodeGenOptions options;
  options.optLevel = LLVMOptLevel::Aggressive;
  options.targetCPU = LLVMTargetCPU::Generic;

  std::string error;
  ASSERT_TRUE(codegen.optimizeModule(options, &error));
  const std::size_t after = codegen.instructionCount();
  ASSERT_TRUE(after <= before);

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("call void @Init()") == std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_print_str") != std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersTensorRandomToShapedRuntimeCall) {
  auto module = std::make_unique<nir::Module>("llvm_tensor_random");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");

  auto randomCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, tensorTy, "rand_tensor");
  randomCall->addOperand(new nir::ConstantString("Tensor.Random"));
  randomCall->addOperand(new nir::ConstantInt(64));
  randomCall->addOperand(new nir::ConstantInt(64));
  entry->addInstruction(std::move(randomCall));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  if (!codegen.verifyModuleIR(&verifyError)) {
    std::cerr << verifyError << std::endl;
    return false;
  }

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_tensor_random_2d") != std::string::npos);
  ASSERT_TRUE(ir.find("call ptr @neuron_tensor_random_2d(i32 64, i32 64)") !=
              std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersTensorLinearFusedCall) {
  auto module = std::make_unique<nir::Module>("llvm_tensor_linear_fused");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("LinearOp", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Argument *bias = fn->addArgument(tensorTy, "bias");
  nir::Argument *res = fn->addArgument(tensorTy, "res");
  nir::Block *entry = fn->createBlock("entry");

  auto fused = std::make_unique<nir::Instruction>(
      nir::InstKind::TensorLinearFused, tensorTy, "fused");
  fused->addOperand(a);
  fused->addOperand(b);
  fused->addOperand(bias);
  fused->addOperand(res);
  entry->addInstruction(std::move(fused));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  if (!codegen.verifyModuleIR(&verifyError)) {
    std::cerr << verifyError << std::endl;
    return false;
  }

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_tensor_linear_fused_ex_hint") != std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersRegisteredFusionBuiltinCall) {
  auto module = std::make_unique<nir::Module>("llvm_fused_conv_bn_relu");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("FusedConv", NType::makeVoid());
  nir::Argument *input = fn->addArgument(tensorTy, "input");
  nir::Argument *kernel = fn->addArgument(tensorTy, "kernel");
  nir::Argument *bias = fn->addArgument(tensorTy, "bias");
  nir::Argument *gamma = fn->addArgument(tensorTy, "gamma");
  nir::Argument *beta = fn->addArgument(tensorTy, "beta");
  nir::Argument *mean = fn->addArgument(tensorTy, "mean");
  nir::Argument *variance = fn->addArgument(tensorTy, "variance");
  nir::Block *entry = fn->createBlock("entry");

  auto fusedCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, tensorTy, "fused");
  fusedCall->addOperand(
      new nir::ConstantString("__neuron_fused_conv2d_batchnorm_relu"));
  fusedCall->addOperand(input);
  fusedCall->addOperand(kernel);
  fusedCall->addOperand(bias);
  fusedCall->addOperand(gamma);
  fusedCall->addOperand(beta);
  fusedCall->addOperand(mean);
  fusedCall->addOperand(variance);
  fusedCall->addOperand(new nir::ConstantFloat(0.001));
  fusedCall->addOperand(new nir::ConstantInt(1));
  fusedCall->addOperand(new nir::ConstantInt(1));
  fusedCall->addOperand(new nir::ConstantInt(0));
  fusedCall->addOperand(new nir::ConstantInt(0));
  fusedCall->addOperand(new nir::ConstantInt(1));
  entry->addInstruction(std::move(fusedCall));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  if (!codegen.verifyModuleIR(&verifyError)) {
    std::cerr << verifyError << std::endl;
    return false;
  }

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_tensor_conv2d_batchnorm_relu_ex_hint") !=
              std::string::npos);
  ASSERT_TRUE(ir.find("call ptr @neuron_tensor_conv2d_batchnorm_relu_ex_hint") !=
              std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersGraphicsV2TypedCallsToRuntime) {
  namespace fs = std::filesystem;
  auto module = std::make_unique<nir::Module>("llvm_graphics_v2");
  const fs::path outputDir =
      fs::temp_directory_path() / "npp_graphics_codegen_wgsl";
  const fs::path cacheDir = outputDir / "cache";
  std::error_code ec;
  fs::remove_all(outputDir, ec);
  fs::create_directories(outputDir, ec);
  fs::create_directories(cacheDir, ec);
  nir::ShaderDesc shaderDesc;
  shaderDesc.name = "BasicLit";
  shaderDesc.hasVertexStage = true;
  shaderDesc.hasFragmentStage = true;
  shaderDesc.vertexLayoutMask = nir::ShaderVertexLayoutPosition;
  shaderDesc.uniformBufferSize = 16;
  shaderDesc.bindings.push_back(
      {"tint", nir::ShaderBindingKind::Vec4, "Color", 0, 0, 0, 16});
  shaderDesc.vertexGlsl =
      "#version 450\n"
      "layout(set = 0, binding = 0, std140) uniform ShaderGlobals {\n"
      "  vec4 tint;\n"
      "} shader_globals;\n"
      "layout(location = 0) in vec3 in_position;\n"
      "void main() {\n"
      "  gl_Position = vec4(in_position, 1.0);\n"
      "}\n";
  shaderDesc.fragmentGlsl =
      "#version 450\n"
      "layout(set = 0, binding = 0, std140) uniform ShaderGlobals {\n"
      "  vec4 tint;\n"
      "} shader_globals;\n"
      "layout(location = 0) out vec4 outColor;\n"
      "void main() {\n"
      "  outColor = shader_globals.tint;\n"
      "}\n";
  ensureShaderCacheArtifact(shaderDesc.vertexGlsl, true);
  ensureShaderCacheArtifact(shaderDesc.fragmentGlsl, false);
  ensureWgslCacheArtifact(shaderDesc.vertexGlsl, true, cacheDir,
                          "@vertex fn main() -> @builtin(position) vec4<f32> {\n"
                          "  return vec4<f32>(0.0, 0.0, 0.0, 1.0);\n"
                          "}\n");
  ensureWgslCacheArtifact(shaderDesc.fragmentGlsl, false, cacheDir,
                          "@fragment fn main() -> @location(0) vec4<f32> {\n"
                          "  return vec4<f32>(1.0, 0.0, 0.0, 1.0);\n"
                          "}\n");
  module->addShader(std::move(shaderDesc));
  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");

  auto windowCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Window"), "window");
  windowCall->addOperand(new nir::ConstantString("Window.Create"));
  windowCall->addOperand(new nir::ConstantInt(800));
  windowCall->addOperand(new nir::ConstantInt(600));
  windowCall->addOperand(new nir::ConstantString("App"));
  nir::Instruction *windowInst = windowCall.get();
  entry->addInstruction(std::move(windowCall));

  auto meshCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Mesh"), "mesh");
  meshCall->addOperand(new nir::ConstantString("Mesh.Load"));
  meshCall->addOperand(new nir::ConstantString("examples/assets/triangle.obj"));
  nir::Instruction *meshInst = meshCall.get();
  entry->addInstruction(std::move(meshCall));

  auto materialCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Material"), "material");
  materialCall->addOperand(new nir::ConstantString("Material.Create"));
  materialCall->addOperand(new nir::ConstantString("BasicLit"));
  nir::Instruction *materialInst = materialCall.get();
  entry->addInstruction(std::move(materialCall));

  auto colorCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Color"), "clear_color");
  colorCall->addOperand(new nir::ConstantString("Color"));
  colorCall->addOperand(new nir::ConstantFloat(0.08));
  colorCall->addOperand(new nir::ConstantFloat(0.08));
  colorCall->addOperand(new nir::ConstantFloat(0.10));
  colorCall->addOperand(new nir::ConstantFloat(1.0));
  nir::Instruction *colorInst = colorCall.get();
  entry->addInstruction(std::move(colorCall));

  auto tintCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeVoid(), "");
  tintCall->addOperand(new nir::ConstantString("material.SetVec4"));
  tintCall->addOperand(materialInst);
  tintCall->addOperand(new nir::ConstantString("tint"));
  tintCall->addOperand(colorInst);
  entry->addInstruction(std::move(tintCall));

  auto clearCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeVoid(), "");
  clearCall->addOperand(new nir::ConstantString("cmd.Clear"));
  clearCall->addOperand(colorInst);
  entry->addInstruction(std::move(clearCall));

  auto drawCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeVoid(), "");
  drawCall->addOperand(new nir::ConstantString("cmd.DrawIndexed"));
  drawCall->addOperand(meshInst);
  drawCall->addOperand(materialInst);
  entry->addInstruction(std::move(drawCall));

  auto presentCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeVoid(), "");
  presentCall->addOperand(new nir::ConstantString("Present"));
  entry->addInstruction(std::move(presentCall));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.setGraphicsShaderOutputDirectory(outputDir);
  codegen.setGraphicsShaderCacheDirectory(cacheDir);
  codegen.setGraphicsShaderAllowCache(true);
  codegen.generate(module.get());

  std::string verifyError;
  if (!codegen.verifyModuleIR(&verifyError)) {
    std::cerr << verifyError << std::endl;
    return false;
  }

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_graphics_create_window") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_mesh_load") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_material_create") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_material_set_vec4") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_color_rgba") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_clear") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_draw_indexed") != std::string::npos);
  ASSERT_TRUE(ir.find("neuron_shader_descriptor") != std::string::npos);
  ASSERT_TRUE(ir.find("neuron_shader_wgsl") != std::string::npos);
  ASSERT_TRUE(fs::exists(outputDir / "BasicLit.vert.wgsl"));
  ASSERT_TRUE(fs::exists(outputDir / "BasicLit.frag.wgsl"));
  ASSERT_TRUE(fs::exists(outputDir / "BasicLit.vert.spv"));
  ASSERT_TRUE(fs::exists(outputDir / "BasicLit.frag.spv"));
  fs::remove_all(outputDir, ec);
  (void)windowInst;
  return true;
}

TEST(LLVMCodegenLowersScene2DCallsToRuntime) {
  auto module = std::make_unique<nir::Module>("llvm_scene2d_runtime");
  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");

  auto sceneCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Scene"), "scene");
  sceneCall->addOperand(new nir::ConstantString("Scene.Create"));
  nir::Instruction *sceneInst = sceneCall.get();
  entry->addInstruction(std::move(sceneCall));

  auto entityCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Entity"), "entity");
  entityCall->addOperand(new nir::ConstantString("scene.CreateEntity"));
  entityCall->addOperand(sceneInst);
  entityCall->addOperand(new nir::ConstantString("Player"));
  nir::Instruction *entityInst = entityCall.get();
  entry->addInstruction(std::move(entityCall));

  auto transformCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Transform"), "transform");
  transformCall->addOperand(new nir::ConstantString("entity.GetTransform"));
  transformCall->addOperand(entityInst);
  nir::Instruction *transformInst = transformCall.get();
  entry->addInstruction(std::move(transformCall));

  auto posCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Vector3"), "position");
  posCall->addOperand(new nir::ConstantString("Vector3"));
  posCall->addOperand(new nir::ConstantFloat(0.0));
  posCall->addOperand(new nir::ConstantFloat(1.0));
  posCall->addOperand(new nir::ConstantFloat(0.0));
  nir::Instruction *posInst = posCall.get();
  entry->addInstruction(std::move(posCall));

  auto setPosition =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeVoid(), "");
  setPosition->addOperand(new nir::ConstantString("transform.SetPosition"));
  setPosition->addOperand(transformInst);
  setPosition->addOperand(posInst);
  entry->addInstruction(std::move(setPosition));

  auto spriteCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("SpriteRenderer2D"), "sprite");
  spriteCall->addOperand(new nir::ConstantString("entity.AddSpriteRenderer2D"));
  spriteCall->addOperand(entityInst);
  nir::Instruction *spriteInst = spriteCall.get();
  entry->addInstruction(std::move(spriteCall));

  auto sizeCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Vector2"), "size");
  sizeCall->addOperand(new nir::ConstantString("Vector2"));
  sizeCall->addOperand(new nir::ConstantFloat(2.0));
  sizeCall->addOperand(new nir::ConstantFloat(3.0));
  nir::Instruction *sizeInst = sizeCall.get();
  entry->addInstruction(std::move(sizeCall));

  auto setSize =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeVoid(), "");
  setSize->addOperand(new nir::ConstantString("sprite.SetSize"));
  setSize->addOperand(spriteInst);
  setSize->addOperand(sizeInst);
  entry->addInstruction(std::move(setSize));

  auto rendererCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Renderer2D"), "renderer");
  rendererCall->addOperand(new nir::ConstantString("Renderer2D.Create"));
  nir::Instruction *rendererInst = rendererCall.get();
  entry->addInstruction(std::move(rendererCall));

  auto colorCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeClass("Color"), "clearColor");
  colorCall->addOperand(new nir::ConstantString("Color"));
  colorCall->addOperand(new nir::ConstantFloat(0.1));
  colorCall->addOperand(new nir::ConstantFloat(0.1));
  colorCall->addOperand(new nir::ConstantFloat(0.12));
  colorCall->addOperand(new nir::ConstantFloat(1.0));
  nir::Instruction *colorInst = colorCall.get();
  entry->addInstruction(std::move(colorCall));

  auto clearCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeVoid(), "");
  clearCall->addOperand(new nir::ConstantString("renderer.SetClearColor"));
  clearCall->addOperand(rendererInst);
  clearCall->addOperand(colorInst);
  entry->addInstruction(std::move(clearCall));

  auto renderCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeVoid(), "");
  renderCall->addOperand(new nir::ConstantString("renderer.Render"));
  renderCall->addOperand(rendererInst);
  renderCall->addOperand(sceneInst);
  entry->addInstruction(std::move(renderCall));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  ASSERT_TRUE(codegen.verifyModuleIR(&verifyError));

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_graphics_scene_create") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_scene_create_entity") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_entity_get_transform") !=
              std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_vector3_create") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_transform_set_position") !=
              std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_entity_add_sprite_renderer2d") !=
              std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_vector2_create") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_sprite_renderer2d_set_size") !=
              std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_renderer2d_create") !=
              std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_renderer2d_set_clear_color") !=
              std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_graphics_renderer2d_render") !=
              std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersTensorExecHintIntoDispatchCalls) {
  auto module = std::make_unique<nir::Module>("llvm_tensor_dispatch_hint");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("DispatchHint", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Argument *c = fn->addArgument(tensorTy, "c");
  nir::Block *entry = fn->createBlock("entry");

  auto gpuAdd =
      std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy, "gpu_add");
  gpuAdd->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  gpuAdd->addOperand(a);
  gpuAdd->addOperand(b);
  entry->addInstruction(std::move(gpuAdd));

  auto autoSub =
      std::make_unique<nir::Instruction>(nir::InstKind::TensorSub, tensorTy, "auto_sub");
  autoSub->addOperand(a);
  autoSub->addOperand(c);
  entry->addInstruction(std::move(autoSub));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  if (!codegen.verifyModuleIR(&verifyError)) {
    std::cerr << verifyError << std::endl;
    return false;
  }

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_tensor_add_ex") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_tensor_sub_ex") != std::string::npos);
  ASSERT_TRUE(ir.find("call ptr @neuron_tensor_add_ex(ptr %a, ptr %b, i32 1)") !=
              std::string::npos);
  ASSERT_TRUE(ir.find("call ptr @neuron_tensor_sub_ex(ptr %a, ptr %c, i32 0)") !=
              std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersTensorMatMulExecHintIntoDispatchCalls) {
  auto module = std::make_unique<nir::Module>("llvm_tensor_matmul_dispatch_hint");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("MatMulDispatchHint", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Argument *bias = fn->addArgument(tensorTy, "bias");
  nir::Argument *res = fn->addArgument(tensorTy, "res");
  nir::Block *entry = fn->createBlock("entry");

  auto matmul =
      std::make_unique<nir::Instruction>(nir::InstKind::TensorMatMul, tensorTy, "mm");
  matmul->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  matmul->addOperand(a);
  matmul->addOperand(b);
  entry->addInstruction(std::move(matmul));

  auto matmulAdd = std::make_unique<nir::Instruction>(
      nir::InstKind::TensorMatMulAdd, tensorTy, "mm_add");
  matmulAdd->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  matmulAdd->addOperand(a);
  matmulAdd->addOperand(b);
  matmulAdd->addOperand(bias);
  entry->addInstruction(std::move(matmulAdd));

  auto fused = std::make_unique<nir::Instruction>(
      nir::InstKind::TensorLinearFused, tensorTy, "fused");
  fused->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  fused->addOperand(a);
  fused->addOperand(b);
  fused->addOperand(bias);
  fused->addOperand(res);
  entry->addInstruction(std::move(fused));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  if (!codegen.verifyModuleIR(&verifyError)) {
    std::cerr << verifyError << std::endl;
    return false;
  }

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_tensor_matmul_ex_hint") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_tensor_matmul_add_ex_hint") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_tensor_linear_fused_ex_hint") != std::string::npos);
  ASSERT_TRUE(ir.find("call ptr @neuron_tensor_matmul_ex_hint(ptr %a, ptr %b, ptr null, i32 0, i32 1)") !=
              std::string::npos);
  ASSERT_TRUE(ir.find("call ptr @neuron_tensor_matmul_add_ex_hint(ptr %a, ptr %b, ptr %bias, i32 1)") !=
              std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersGpuScopeMarkersToRuntimeCalls) {
  auto module = std::make_unique<nir::Module>("llvm_gpu_scope_markers");
  auto tensorTy = NType::makeTensor(NType::makeFloat());

  nir::Function *fn = module->createFunction("ScopedGpu", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Block *entry = fn->createBlock("entry");

  auto scopeBegin =
      std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeBegin,
                                         NType::makeVoid(), "");
  entry->addInstruction(std::move(scopeBegin));

  auto add =
      std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy, "sum");
  add->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  add->addOperand(a);
  add->addOperand(b);
  entry->addInstruction(std::move(add));

  auto scopeEnd =
      std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeEnd,
                                         NType::makeVoid(), "");
  entry->addInstruction(std::move(scopeEnd));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  if (!codegen.verifyModuleIR(&verifyError)) {
    std::cerr << verifyError << std::endl;
    return false;
  }

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_gpu_scope_begin") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_gpu_scope_end") != std::string::npos);
  ASSERT_TRUE(ir.find("call i32 @neuron_gpu_scope_begin()") != std::string::npos);
  ASSERT_TRUE(ir.find("call i32 @neuron_gpu_scope_end()") != std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersGpuScopeSelectorToRuntimeScopeBeginEx) {
  auto module = std::make_unique<nir::Module>("llvm_gpu_scope_selector");
  nir::Function *fn = module->createFunction("ScopedGpuSelector", NType::makeVoid());
  nir::Block *entry = fn->createBlock("entry");

  auto scopeBegin =
      std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeBegin,
                                         NType::makeVoid(), "");
  scopeBegin->addOperand(new nir::ConstantInt(1));
  scopeBegin->addOperand(new nir::ConstantInt(2));
  entry->addInstruction(std::move(scopeBegin));

  auto scopeEnd =
      std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeEnd,
                                         NType::makeVoid(), "");
  entry->addInstruction(std::move(scopeEnd));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  if (!codegen.verifyModuleIR(&verifyError)) {
    std::cerr << verifyError << std::endl;
    return false;
  }

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_gpu_scope_begin_ex") != std::string::npos);
  ASSERT_TRUE(ir.find("call i32 @neuron_gpu_scope_begin_ex(i32 1, i32 2)") !=
              std::string::npos);
  return true;
}

TEST(LLVMCodegenPreservesLiftedGpuScopeAroundLoopTensorUpdates) {
  auto module = std::make_unique<nir::Module>("llvm_gpu_scope_lift_codegen");
  auto tensorTy = NType::makeTensor(NType::makeFloat());
  nir::Function *fn = module->createFunction("Init", NType::makeVoid());
  nir::Argument *a = fn->addArgument(tensorTy, "a");
  nir::Argument *b = fn->addArgument(tensorTy, "b");
  nir::Block *entry = fn->createBlock("entry");
  nir::Block *cond = fn->createBlock("cond");
  nir::Block *body = fn->createBlock("body");
  nir::Block *inc = fn->createBlock("inc");
  nir::Block *exit = fn->createBlock("exit");

  auto brEntry =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brEntry->addOperand(new nir::BlockRef(cond));
  entry->addInstruction(std::move(brEntry));

  auto condBr = std::make_unique<nir::Instruction>(nir::InstKind::CondBr,
                                                   NType::makeVoid(), "");
  condBr->addOperand(new nir::ConstantInt(1));
  condBr->addOperand(new nir::BlockRef(body));
  condBr->addOperand(new nir::BlockRef(exit));
  cond->addInstruction(std::move(condBr));

  auto scopeBegin = std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeBegin,
                                                       NType::makeVoid(), "");
  scopeBegin->addOperand(new nir::ConstantInt(0));
  scopeBegin->addOperand(new nir::ConstantInt(0));
  body->addInstruction(std::move(scopeBegin));

  auto add =
      std::make_unique<nir::Instruction>(nir::InstKind::TensorAdd, tensorTy, "sum");
  add->setExecutionHint(nir::ExecutionHint::GpuPrefer);
  add->addOperand(a);
  add->addOperand(b);
  body->addInstruction(std::move(add));

  auto scopeEnd = std::make_unique<nir::Instruction>(nir::InstKind::GpuScopeEnd,
                                                     NType::makeVoid(), "");
  body->addInstruction(std::move(scopeEnd));

  auto brBody =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brBody->addOperand(new nir::BlockRef(inc));
  body->addInstruction(std::move(brBody));

  auto brInc =
      std::make_unique<nir::Instruction>(nir::InstKind::Br, NType::makeVoid(), "");
  brInc->addOperand(new nir::BlockRef(cond));
  inc->addInstruction(std::move(brInc));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  exit->addInstruction(std::move(ret));

  nir::GpuScopeLiftingPass pass;
  ASSERT_TRUE(pass.runOnModule(module.get()));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  if (!codegen.verifyModuleIR(&verifyError)) {
    std::cerr << verifyError << std::endl;
    return false;
  }

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  const std::string scopeBeginCall = "call i32 @neuron_gpu_scope_begin_ex(i32 0, i32 0)";
  const std::string tensorAddCall = "call ptr @neuron_tensor_add_ex(ptr %a, ptr %b, i32 1)";
  const std::string scopeEndCall = "call i32 @neuron_gpu_scope_end()";

  const size_t beginPos = ir.find(scopeBeginCall);
  const size_t addPos = ir.find(tensorAddCall);
  const size_t endPos = ir.find(scopeEndCall);
  ASSERT_TRUE(beginPos != std::string::npos);
  ASSERT_TRUE(addPos != std::string::npos);
  ASSERT_TRUE(endPos != std::string::npos);
  ASSERT_TRUE(beginPos < addPos);
  ASSERT_TRUE(addPos < endPos);
  return true;
}

TEST(LLVMCodegenKeepsExternFunctionAsDeclaration) {
  auto module = std::make_unique<nir::Module>("llvm_extern_decl");
  auto *externFn = module->createFunction("c_add", NType::makeInt(), true);
  externFn->addArgument(NType::makeInt(), "a");
  externFn->addArgument(NType::makeInt(), "b");

  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");

  auto call =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeInt(), "sum");
  call->addOperand(new nir::ConstantString("c_add"));
  call->addOperand(new nir::ConstantInt(1));
  call->addOperand(new nir::ConstantInt(2));
  entry->addInstruction(std::move(call));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  ASSERT_TRUE(codegen.verifyModuleIR(&verifyError));

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("declare i64 @c_add(i64, i64)") != std::string::npos);
  ASSERT_TRUE(ir.find("define i64 @c_add") == std::string::npos);
  return true;
}

TEST(LLVMCodegenRegistersAndInvokesModuleCppExports) {
  auto module = std::make_unique<nir::Module>("llvm_modulecpp");

  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");
  auto call = std::make_unique<nir::Instruction>(nir::InstKind::Call,
                                                 NType::makeString(), "version");
  call->addOperand(new nir::ConstantString("Tensorflow.Version"));
  entry->addInstruction(std::move(call));
  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.setModuleCppExports(
      {{"Tensorflow.Version",
        ModuleCppCompileExport{"Tensorflow.Version",
                               "native_modules/Tensorflow/tensorflow.dll",
                               "tf_version", {}, "string"}}});
  codegen.generate(module.get());

  std::string verifyError;
  ASSERT_TRUE(codegen.verifyModuleIR(&verifyError));

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_modulecpp_register") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_modulecpp_startup") != std::string::npos);
  ASSERT_TRUE(ir.find("@neuron_modulecpp_invoke") != std::string::npos);
  ASSERT_TRUE(ir.find("Tensorflow.Version") != std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersThreadCallToRuntimeSubmit) {
  auto module = std::make_unique<nir::Module>("llvm_thread_submit");
  auto *worker = module->createFunction("Worker", NType::makeVoid());
  nir::Block *workerEntry = worker->createBlock("entry");
  auto workerRet =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  workerEntry->addInstruction(std::move(workerRet));

  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");
  auto threadCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeInt(), "thread_h");
  threadCall->addOperand(new nir::ConstantString("thread"));
  threadCall->addOperand(new nir::ConstantString("Worker"));
  entry->addInstruction(std::move(threadCall));
  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  ASSERT_TRUE(codegen.verifyModuleIR(&verifyError));

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_thread_submit") != std::string::npos);
  ASSERT_TRUE(ir.find("__neuron_thread_thunk_Worker") != std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersSmartInputIntCallToRuntime) {
  auto module = std::make_unique<nir::Module>("llvm_input_int");
  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");

  auto inputCall =
      std::make_unique<nir::Instruction>(nir::InstKind::Call, NType::makeInt(), "age");
  inputCall->addOperand(new nir::ConstantString("__neuron_input_int"));
  inputCall->addOperand(new nir::ConstantString("Age: "));
  inputCall->addOperand(new nir::ConstantInt(1));
  inputCall->addOperand(new nir::ConstantInt(18));
  inputCall->addOperand(new nir::ConstantInt(1));
  inputCall->addOperand(new nir::ConstantInt(99));
  inputCall->addOperand(new nir::ConstantInt(1));
  inputCall->addOperand(new nir::ConstantInt(21));
  inputCall->addOperand(new nir::ConstantInt(5000));
  entry->addInstruction(std::move(inputCall));
  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  ASSERT_TRUE(codegen.verifyModuleIR(&verifyError));

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_io_input_int") != std::string::npos);
  ASSERT_TRUE(ir.find("call i64 @neuron_io_input_int") != std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersSmartInputStringCallToRuntime) {
  auto module = std::make_unique<nir::Module>("llvm_input_string");
  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");

  auto inputCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeString(), "password");
  inputCall->addOperand(new nir::ConstantString("__neuron_input_string"));
  inputCall->addOperand(new nir::ConstantString("Password: "));
  inputCall->addOperand(new nir::ConstantInt(1));
  inputCall->addOperand(new nir::ConstantInt(1));
  inputCall->addOperand(new nir::ConstantString("guest"));
  inputCall->addOperand(new nir::ConstantInt(1500));
  entry->addInstruction(std::move(inputCall));
  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  ASSERT_TRUE(codegen.verifyModuleIR(&verifyError));

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_io_input_string") != std::string::npos);
  ASSERT_TRUE(ir.find("call ptr @neuron_io_input_string") != std::string::npos);
  return true;
}

TEST(LLVMCodegenLowersSmartInputEnumCallToRuntime) {
  auto module = std::make_unique<nir::Module>("llvm_input_enum");
  nir::Function *init = module->createFunction("Init", NType::makeVoid());
  nir::Block *entry = init->createBlock("entry");

  auto inputCall = std::make_unique<nir::Instruction>(
      nir::InstKind::Call, NType::makeEnum("Color"), "choice");
  inputCall->addOperand(new nir::ConstantString("__neuron_input_enum"));
  inputCall->addOperand(new nir::ConstantString("Color: "));
  inputCall->addOperand(new nir::ConstantString("Red\nGreen\nBlue"));
  inputCall->addOperand(new nir::ConstantInt(3));
  inputCall->addOperand(new nir::ConstantInt(1));
  inputCall->addOperand(new nir::ConstantInt(2));
  inputCall->addOperand(new nir::ConstantInt(1200));
  entry->addInstruction(std::move(inputCall));
  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeVoid(), "");
  entry->addInstruction(std::move(ret));

  LLVMCodeGen codegen;
  codegen.generate(module.get());

  std::string verifyError;
  ASSERT_TRUE(codegen.verifyModuleIR(&verifyError));

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  codegen.getLLVMModule()->print(irStream, nullptr);
  irStream.flush();

  ASSERT_TRUE(ir.find("@neuron_io_input_enum") != std::string::npos);
  ASSERT_TRUE(ir.find("call i64 @neuron_io_input_enum") != std::string::npos);
  return true;
}

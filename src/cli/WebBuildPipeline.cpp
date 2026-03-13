#include "neuronc/cli/WebBuildPipeline.h"

#include "neuronc/cli/WebShaderTranspiler.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace neuron::cli {

namespace {

std::string quoteArg(const std::string &value) {
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

std::string loadTextFile(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return std::string();
  }
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

bool writeTextFile(const fs::path &path, const std::string &text,
                   std::string *outError) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create directory for file '" + path.string() +
                  "': " + ec.message();
    }
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to write file: " + path.string();
    }
    return false;
  }

  out << text;
  if (!out.good()) {
    if (outError != nullptr) {
      *outError = "failed to flush file: " + path.string();
    }
    return false;
  }
  return true;
}

void replaceAll(std::string *text, const std::string &needle,
                const std::string &replacement) {
  if (text == nullptr || needle.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = text->find(needle, pos)) != std::string::npos) {
    text->replace(pos, needle.size(), replacement);
    pos += replacement.size();
  }
}

std::string defaultIndexTemplate() {
  return "<!doctype html>\n"
         "<html lang=\"en\">\n"
         "  <head>\n"
         "    <meta charset=\"utf-8\">\n"
         "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
         "    <title>Neuron Web App</title>\n"
         "    <style>html,body{margin:0;padding:0;height:100%;background:#111;color:#eee;font-family:ui-sans-serif,system-ui,sans-serif;}#{{CANVAS_ID}}{display:block;width:100vw;height:100vh;}</style>\n"
         "  </head>\n"
         "  <body>\n"
         "    <canvas id=\"{{CANVAS_ID}}\"></canvas>\n"
         "    <script src=\"loader.js\"></script>\n"
         "    <script src=\"app.js\"></script>\n"
         "  </body>\n"
         "</html>\n";
}

std::string defaultLoaderTemplate() {
  return "(function() {\n"
         "  var canvas = document.getElementById('{{CANVAS_ID}}');\n"
         "  if (!canvas) {\n"
         "    canvas = document.createElement('canvas');\n"
         "    canvas.id = '{{CANVAS_ID}}';\n"
         "    document.body.appendChild(canvas);\n"
         "  }\n"
         "  window.Module = window.Module || {};\n"
         "  window.Module.canvas = canvas;\n"
         "})();\n";
}

std::vector<fs::path> collectSpirvShaders(const fs::path &projectRoot) {
  std::vector<fs::path> files;
  std::error_code ec;
  if (!fs::exists(projectRoot, ec)) {
    return files;
  }
  for (fs::recursive_directory_iterator it(projectRoot, ec), end; it != end && !ec;
       it.increment(ec)) {
    if (!it->is_regular_file()) {
      continue;
    }
    const fs::path path = it->path();
    if (path.extension() != ".spv") {
      continue;
    }
    files.push_back(path);
  }
  std::sort(files.begin(), files.end());
  return files;
}

std::vector<fs::path> runtimeWebSources(const fs::path &toolRoot) {
  return {
      toolRoot / "runtime/src/runtime.c",
      toolRoot / "runtime/src/io.c",
      toolRoot / "runtime/src/nn.c",
      toolRoot / "runtime/src/gpu.c",
      toolRoot / "runtime/src/tensor.c",
      toolRoot / "runtime/src/tensor/tensor_config.c",
      toolRoot / "runtime/src/tensor/tensor_core.c",
      toolRoot / "runtime/src/tensor/tensor_math.c",
      toolRoot / "runtime/src/tensor/tensor_microkernel.c",
      toolRoot / "runtime/src/platform/platform_manager.c",
      toolRoot / "runtime/src/platform/common/platform_error.c",
      toolRoot / "runtime/src/graphics/graphics_core_state.c",
      toolRoot / "runtime/src/graphics/graphics_core_window_canvas_api.c",
      toolRoot / "runtime/src/graphics/graphics_core_assets_api.c",
      toolRoot / "runtime/src/graphics/graphics_window_canvas.c",
      toolRoot / "runtime/src/graphics/graphics_assets.c",
      toolRoot / "runtime/src/graphics/scene2d/graphics_scene2d_scene.c",
      toolRoot / "runtime/src/graphics/scene2d/graphics_scene2d_renderer.c",
      toolRoot / "runtime/src/graphics/assets/graphics_asset_obj_parser.c",
      toolRoot / "runtime/src/graphics/backend/webgpu/graphics_webgpu_backend.c",
      toolRoot / "runtime/src/graphics/backend/stub/graphics_vk_backend_stub.c",
      toolRoot / "runtime/src/platform/web/platform_web_library.c",
      toolRoot / "runtime/src/platform/web/platform_web_env.c",
      toolRoot / "runtime/src/platform/web/platform_web_time.c",
      toolRoot / "runtime/src/platform/web/platform_web_path.c",
      toolRoot / "runtime/src/platform/web/platform_web_diagnostics.c",
      toolRoot / "runtime/src/platform/web/platform_web_process.c",
      toolRoot / "runtime/src/platform/web/platform_web_thread.c",
      toolRoot / "runtime/src/platform/web/platform_web_window.c",
      toolRoot / "runtime/src/platform/gpu_webgpu.c",
  };
}

bool isWebAssetExtension(const fs::path &path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return ext == ".obj" || ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

bool shouldSkipAssetDirectory(const fs::path &path) {
  const std::string name = path.filename().string();
  return name == ".git" || name == "build" || name == ".vs" ||
         name == "node_modules";
}

std::vector<fs::path> collectWebAssetFiles(const fs::path &projectRoot) {
  std::vector<fs::path> files;
  std::error_code ec;
  if (!fs::exists(projectRoot, ec)) {
    return files;
  }

  fs::recursive_directory_iterator it(projectRoot, ec), end;
  while (it != end && !ec) {
    const fs::path current = it->path();
    if (it->is_directory(ec)) {
      if (shouldSkipAssetDirectory(current)) {
        it.disable_recursion_pending();
      }
      it.increment(ec);
      continue;
    }
    if (!it->is_regular_file(ec) || !isWebAssetExtension(current)) {
      it.increment(ec);
      continue;
    }
    files.push_back(current);
    it.increment(ec);
  }

  std::sort(files.begin(), files.end());
  return files;
}

} // namespace

WebBuildResult runWebBuildPipeline(const WebBuildRequest &request,
                                   const WebBuildPipelineDependencies &deps) {
  WebBuildResult result;
  result.devServerPort = request.projectConfig.web.devServerPort;

  if (!deps.compileToObject || !deps.resolveToolCommand || !deps.runSystemCommand ||
      !deps.quotePath) {
    result.error = "web build dependencies are incomplete";
    return result;
  }

  const fs::path projectRoot = request.projectRoot.empty()
                                   ? fs::current_path()
                                   : request.projectRoot;
  const fs::path buildRoot = projectRoot /
      (request.projectConfig.buildDir.empty() ? fs::path("build")
                                               : fs::path(request.projectConfig.buildDir));
  const fs::path outputDir = buildRoot / "web";
  const fs::path objectDir = outputDir / "obj";
  const fs::path shaderOutDir = outputDir / "shaders";
  const fs::path shaderCacheDir = buildRoot / "web_shader_cache";
  std::error_code ec;
  fs::create_directories(objectDir, ec);
  if (ec) {
    result.error = "failed to create web object directory '" +
                   objectDir.string() + "': " + ec.message();
    return result;
  }
  ec.clear();
  fs::create_directories(shaderOutDir, ec);
  if (ec) {
    result.error = "failed to create web shader output directory '" +
                   shaderOutDir.string() + "': " + ec.message();
    return result;
  }

  const std::string mainFile = request.projectConfig.mainFile.empty()
                                   ? "src/Main.nr"
                                   : request.projectConfig.mainFile;
  const fs::path mainPath = (projectRoot / mainFile).lexically_normal();
  if (!fs::exists(mainPath)) {
    result.error = "web build entry file not found: " + mainPath.string();
    return result;
  }

  CompilePipelineOptions compileOptions;
  compileOptions.targetTripleOverride = "wasm32-unknown-emscripten";
  compileOptions.enableWasmSimd = request.projectConfig.web.wasmSimd;
  compileOptions.linkExecutable = false;
  compileOptions.outputDirOverride = objectDir;
  compileOptions.graphicsShaderOutputDir = shaderOutDir;
  compileOptions.graphicsShaderCacheDir = shaderCacheDir;
  compileOptions.graphicsShaderAllowCache = request.projectConfig.web.wgslCache;
  compileOptions.outputStemOverride = "program";
  compileOptions.objectExtension = ".o";
  compileOptions.executableExtension = ".js";

  std::string programObject;
  const int compileRc = deps.compileToObject(mainPath.string(), compileOptions,
                                             &programObject);
  if (compileRc != 0 || programObject.empty()) {
    result.error = "web build failed while compiling program object";
    return result;
  }

  const fs::path programObjectPath(programObject);
  if (!fs::exists(programObjectPath)) {
    result.error = "web build object output is missing: " +
                   programObjectPath.string();
    return result;
  }

  const std::vector<fs::path> shaderInputs = collectSpirvShaders(projectRoot);
  for (const fs::path &shaderInput : shaderInputs) {
    WebShaderTranspileOptions transpileOptions;
    transpileOptions.cacheDirectory = shaderCacheDir;
    transpileOptions.allowCache = request.projectConfig.web.wgslCache;
    WebShaderTranspileResult transpileResult;
    if (!transpileSpirvToWgsl(shaderInput, transpileOptions, &transpileResult)) {
      result.warnings.push_back("shader transpile failed for " +
                                shaderInput.string() + ": " +
                                transpileResult.error);
      continue;
    }

    fs::path relativeName = shaderInput.filename();
    relativeName.replace_extension(".wgsl");
    const fs::path outputShaderPath = shaderOutDir / relativeName;
    std::string writeError;
    if (!writeTextFile(outputShaderPath, transpileResult.outputText,
                       &writeError)) {
      result.warnings.push_back(writeError);
    }
  }

  const std::string emxx = deps.resolveToolCommand("em++");
  const fs::path appJsPath = outputDir / "app.js";
  std::ostringstream linkCmd;
  linkCmd << (emxx.empty() ? std::string("em++") : quoteArg(emxx));
  linkCmd << " -O3";
  if (request.projectConfig.web.wasmSimd) {
    linkCmd << " -msimd128";
  }
  if (request.projectConfig.web.enableSharedArray) {
    linkCmd << " -pthread -sUSE_PTHREADS=1 -sPTHREAD_POOL_SIZE=4";
  }

  const uint64_t initialMemoryBytes =
      static_cast<uint64_t>(request.projectConfig.web.initialMemoryMb) *
      1024ull * 1024ull;
  const uint64_t maxMemoryBytes =
      static_cast<uint64_t>(request.projectConfig.web.maximumMemoryMb) *
      1024ull * 1024ull;

  linkCmd << " -sWASM=1 -sALLOW_MEMORY_GROWTH=1";
  linkCmd << " -sUSE_WEBGPU=1";
  linkCmd << " -sINITIAL_MEMORY=" << initialMemoryBytes;
  linkCmd << " -sMAXIMUM_MEMORY=" << std::max(maxMemoryBytes, initialMemoryBytes);
  linkCmd << " -sENVIRONMENT=web";
  linkCmd << " -DNeuron_ENABLE_CUDA_BACKEND=0";
  linkCmd << " -DNeuron_ENABLE_VULKAN_BACKEND=0";
  linkCmd << " -DNeuron_ENABLE_WEBGPU_BACKEND=1";
  linkCmd << " -I" << deps.quotePath(request.toolRoot / "runtime/include");
  linkCmd << " -I" << deps.quotePath(request.toolRoot / "runtime/src");
  linkCmd << " " << deps.quotePath(programObjectPath);

  const std::vector<fs::path> runtimeSources = runtimeWebSources(request.toolRoot);
  for (const fs::path &source : runtimeSources) {
    if (!fs::exists(source)) {
      result.error = "required runtime source missing for web build: " +
                     source.string();
      return result;
    }
    linkCmd << " " << deps.quotePath(source);
  }

  const std::vector<fs::path> assetFiles = collectWebAssetFiles(projectRoot);
  for (const fs::path &asset : assetFiles) {
    std::error_code relativeEc;
    const fs::path relativeAsset = fs::relative(asset, projectRoot, relativeEc);
    if (relativeEc || relativeAsset.empty()) {
      continue;
    }
    const fs::path copiedAssetPath = outputDir / relativeAsset;
    std::error_code copyEc;
    fs::create_directories(copiedAssetPath.parent_path(), copyEc);
    if (!copyEc) {
      fs::copy_file(asset, copiedAssetPath,
                    fs::copy_options::overwrite_existing, copyEc);
    }
    linkCmd << " --preload-file "
            << quoteArg(asset.string() + "@" +
                        relativeAsset.generic_string());
  }
  linkCmd << " -o " << deps.quotePath(appJsPath);

  if (request.verbose) {
    std::cout << "Web link: " << linkCmd.str() << std::endl;
  }

  if (deps.runSystemCommand(linkCmd.str()) != 0) {
    result.error = "web link failed; ensure Emscripten tools are available (em++ in PATH)";
    return result;
  }

  const fs::path appWasmPath = outputDir / "app.wasm";
  if (!fs::exists(appJsPath) || !fs::exists(appWasmPath)) {
    result.error = "web build completed without expected app.js/app.wasm artifacts";
    return result;
  }

  const fs::path indexTemplatePath =
      request.toolRoot / "src/cli/templates/web/index.template.html";
  const fs::path loaderTemplatePath =
      request.toolRoot / "src/cli/templates/web/loader.template.js";

  std::string indexHtml = loadTextFile(indexTemplatePath);
  std::string loaderJs = loadTextFile(loaderTemplatePath);
  if (indexHtml.empty()) {
    indexHtml = defaultIndexTemplate();
  }
  if (loaderJs.empty()) {
    loaderJs = defaultLoaderTemplate();
  }

  replaceAll(&indexHtml, "{{CANVAS_ID}}", request.projectConfig.web.canvasId);
  replaceAll(&loaderJs, "{{CANVAS_ID}}", request.projectConfig.web.canvasId);

  const fs::path indexHtmlPath = outputDir / "index.html";
  const fs::path loaderJsPath = outputDir / "loader.js";
  std::string writeError;
  if (!writeTextFile(indexHtmlPath, indexHtml, &writeError)) {
    result.error = writeError;
    return result;
  }
  if (!writeTextFile(loaderJsPath, loaderJs, &writeError)) {
    result.error = writeError;
    return result;
  }

  result.success = true;
  result.outputDirectory = outputDir;
  result.htmlEntryPath = indexHtmlPath;
  result.loaderJsPath = loaderJsPath;
  result.wasmJsPath = appJsPath;
  result.wasmBinaryPath = appWasmPath;
  return result;
}

} // namespace neuron::cli

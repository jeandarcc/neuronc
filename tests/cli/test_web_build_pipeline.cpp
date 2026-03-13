#include "neuronc/cli/WebBuildPipeline.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string quoteWebBuildTestPath(const std::filesystem::path &path) {
  return "\"" + path.string() + "\"";
}

bool writeWebBuildTestFile(const std::filesystem::path &path,
                           const std::string &text) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out << text;
  return out.good();
}

} // namespace

TEST(WebBuildPipelinePassesGraphicsShaderOptionsAndRuntimeSources) {
  namespace fs = std::filesystem;

  const auto uniqueSuffix =
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const fs::path root = fs::temp_directory_path() /
                        ("npp_web_build_pipeline_graphics_" + uniqueSuffix);
  std::error_code ec;
  fs::remove_all(root, ec);
  ASSERT_TRUE(writeWebBuildTestFile(root / "src" / "Main.nr",
                                    "Init method() {\n}\n"));
  ASSERT_TRUE(writeWebBuildTestFile(root / "examples" / "assets" / "quad.obj",
                                    "v 0 0 0\n"));
  ASSERT_TRUE(writeWebBuildTestFile(root / "examples" / "assets" / "checker.png",
                                    "png"));

  neuron::ProjectConfig config;
  config.mainFile = "src/Main.nr";
  config.buildDir = "build";
  config.web.wgslCache = false;

  neuron::cli::WebBuildRequest request;
  request.toolRoot = fs::current_path();
  request.projectRoot = root;
  request.projectConfig = config;

  std::string linkCommand;
  neuron::cli::CompilePipelineOptions capturedOptions;
  bool compileCalled = false;

  neuron::cli::WebBuildPipelineDependencies deps;
  deps.compileToObject =
      [&](const std::string &filepath,
          const neuron::cli::CompilePipelineOptions &options,
          std::string *outObjectPath) -> int {
    (void)filepath;
    compileCalled = true;
    capturedOptions = options;
    if (options.graphicsShaderOutputDir.empty() ||
        options.graphicsShaderCacheDir.empty() ||
        options.graphicsShaderAllowCache) {
      return 1;
    }

    const fs::path objectPath = options.outputDirOverride / "program.o";
    if (!writeWebBuildTestFile(objectPath, "object") ||
        !writeWebBuildTestFile(
            options.graphicsShaderOutputDir / "BasicLit.vert.wgsl",
            "@vertex fn main() {}") ||
        !writeWebBuildTestFile(
            options.graphicsShaderOutputDir / "BasicLit.frag.wgsl",
            "@fragment fn main() {}")) {
      return 1;
    }
    if (outObjectPath != nullptr) {
      *outObjectPath = objectPath.string();
    }
    return 0;
  };
  deps.resolveToolCommand = [](const std::string &tool) { return tool; };
  deps.runSystemCommand = [&](const std::string &command) -> int {
    linkCommand = command;
    if (!writeWebBuildTestFile(root / "build" / "web" / "app.js",
                               "console.log('ok');") ||
        !writeWebBuildTestFile(root / "build" / "web" / "app.wasm", "wasm")) {
      return 1;
    }
    return 0;
  };
  deps.quotePath = [](const fs::path &path) { return quoteWebBuildTestPath(path); };

  const neuron::cli::WebBuildResult result =
      neuron::cli::runWebBuildPipeline(request, deps);

  ASSERT_TRUE(compileCalled);
  ASSERT_TRUE(result.success);
  ASSERT_TRUE(linkCommand.find("graphics_core_window_canvas_api.c") !=
              std::string::npos);
  ASSERT_TRUE(linkCommand.find("graphics_webgpu_backend.c") !=
              std::string::npos);
  ASSERT_TRUE(linkCommand.find("-sUSE_WEBGPU=1") != std::string::npos);
  ASSERT_TRUE(linkCommand.find("--preload-file") != std::string::npos);
  ASSERT_TRUE(fs::exists(root / "build" / "web" / "shaders" / "BasicLit.vert.wgsl"));
  ASSERT_TRUE(fs::exists(root / "build" / "web" / "shaders" / "BasicLit.frag.wgsl"));
  ASSERT_TRUE(fs::exists(root / "build" / "web" / "examples" / "assets" / "quad.obj"));
  ASSERT_TRUE(fs::exists(root / "build" / "web" / "examples" / "assets" / "checker.png"));

  fs::remove_all(root, ec);
  return true;
}

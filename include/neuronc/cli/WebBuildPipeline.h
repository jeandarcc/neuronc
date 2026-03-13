#pragma once

#include "neuronc/cli/ProjectConfig.h"
#include "neuronc/cli/pipeline/CompilePipeline.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace neuron::cli {

struct WebBuildRequest {
  std::filesystem::path toolRoot;
  std::filesystem::path projectRoot;
  neuron::ProjectConfig projectConfig;
  bool verbose = false;
};

struct WebBuildPipelineDependencies {
  std::function<int(const std::string &, const CompilePipelineOptions &,
                    std::string *)>
      compileToObject;
  std::function<std::string(const std::string &)> resolveToolCommand;
  std::function<int(const std::string &)> runSystemCommand;
  std::function<std::string(const std::filesystem::path &)> quotePath;
};

struct WebBuildResult {
  bool success = false;
  std::filesystem::path outputDirectory;
  std::filesystem::path htmlEntryPath;
  std::filesystem::path loaderJsPath;
  std::filesystem::path wasmJsPath;
  std::filesystem::path wasmBinaryPath;
  int devServerPort = 8080;
  std::vector<std::string> warnings;
  std::string error;
};

WebBuildResult runWebBuildPipeline(const WebBuildRequest &request,
                                   const WebBuildPipelineDependencies &deps);

} // namespace neuron::cli

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace neuron::cli {

struct WebShaderTranspileOptions {
  std::filesystem::path cacheDirectory;
  std::vector<std::string> preferredTools;
  bool allowCache = true;
};

struct WebShaderTranspileResult {
  bool success = false;
  bool cacheHit = false;
  std::filesystem::path outputPath;
  std::string outputText;
  std::string error;
};

bool transpileSpirvToWgsl(const std::filesystem::path &inputSpirv,
                          const WebShaderTranspileOptions &options,
                          WebShaderTranspileResult *outResult);

} // namespace neuron::cli
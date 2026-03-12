#include "neuronc/cli/WebShaderTranspiler.h"

#include "neuronc/ncon/Sha256.h"

#include <cstdlib>
#include <fstream>
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

bool readFileBytes(const fs::path &path, std::vector<uint8_t> *outBytes) {
  if (outBytes == nullptr) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  outBytes->assign(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());
  return true;
}

std::string readTextFile(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return std::string();
  }
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

bool executeCommand(const std::string &command) {
  const int exitCode = std::system(command.c_str());
  return exitCode == 0;
}

std::string buildTranspileCommand(const std::string &tool,
                                  const fs::path &inputPath,
                                  const fs::path &outputPath) {
  const std::string toolLower = [&]() {
    std::string lowered = tool;
    for (char &ch : lowered) {
      if (ch >= 'A' && ch <= 'Z') {
        ch = static_cast<char>(ch - 'A' + 'a');
      }
    }
    return lowered;
  }();

  const std::string toolArg = quoteArg(tool);
  const std::string inputArg = quoteArg(inputPath.string());
  const std::string outputArg = quoteArg(outputPath.string());

  if (toolLower.find("spirv-cross") != std::string::npos) {
    return toolArg + " --wgsl " + inputArg + " --output " + outputArg;
  }

  if (toolLower.find("tint") != std::string::npos) {
    return toolArg + " --format wgsl -o " + outputArg + " " + inputArg;
  }

  return toolArg + " " + inputArg + " " + outputArg;
}

std::vector<std::string> defaultTools() {
#if defined(_WIN32)
  return {"spirv-cross.exe", "tint.exe", "spirv-cross", "tint"};
#else
  return {"spirv-cross", "tint"};
#endif
}

} // namespace

bool transpileSpirvToWgsl(const fs::path &inputSpirv,
                          const WebShaderTranspileOptions &options,
                          WebShaderTranspileResult *outResult) {
  if (outResult == nullptr) {
    return false;
  }
  *outResult = WebShaderTranspileResult{};

  if (inputSpirv.empty() || !fs::exists(inputSpirv)) {
    outResult->error = "input SPIR-V file not found: " + inputSpirv.string();
    return false;
  }

  std::vector<uint8_t> fileBytes;
  if (!readFileBytes(inputSpirv, &fileBytes) || fileBytes.empty()) {
    outResult->error = "failed to read input SPIR-V bytes: " + inputSpirv.string();
    return false;
  }

  const std::string digest = neuron::ncon::sha256Hex(fileBytes);
  const fs::path cacheDir = options.cacheDirectory.empty()
                                ? fs::path("build") / "web_shader_cache"
                                : options.cacheDirectory;
  std::error_code ec;
  fs::create_directories(cacheDir, ec);

  const fs::path outputPath = cacheDir / (digest + ".wgsl");
  outResult->outputPath = outputPath;

  if (options.allowCache && fs::exists(outputPath)) {
    outResult->outputText = readTextFile(outputPath);
    if (!outResult->outputText.empty()) {
      outResult->success = true;
      outResult->cacheHit = true;
      return true;
    }
  }

  std::vector<std::string> tools = options.preferredTools;
  if (tools.empty()) {
    tools = defaultTools();
  }

  for (const std::string &tool : tools) {
    const std::string command =
        buildTranspileCommand(tool, inputSpirv, outputPath);
    if (!executeCommand(command)) {
      continue;
    }

    const std::string outputText = readTextFile(outputPath);
    if (outputText.empty()) {
      continue;
    }

    outResult->outputText = outputText;
    outResult->success = true;
    outResult->cacheHit = false;
    return true;
  }

  outResult->error =
      "failed to transpile SPIR-V to WGSL with configured tools";
  return false;
}

} // namespace neuron::cli

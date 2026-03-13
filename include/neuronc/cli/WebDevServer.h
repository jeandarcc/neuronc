#pragma once

#include <filesystem>
#include <string>

namespace neuron::cli {

struct WebDevServerOptions {
  std::filesystem::path rootDirectory;
  int port = 8080;
  bool openBrowser = true;
};

int runWebDevServer(const WebDevServerOptions &options,
                    std::string *outError = nullptr);

} // namespace neuron::cli

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace neuron::ncon {

enum class MiniCliActionKind {
  ShowHelp,
  RunContainer,
  RunEmbedded,
  RunSandboxHelper,
  Error,
};

struct MiniCliAction {
  MiniCliActionKind kind = MiniCliActionKind::Error;
  std::filesystem::path containerPath;
  std::string error;
};

MiniCliAction parseMiniCliAction(const std::vector<std::string> &args);
int runMiniCli(int argc, char *argv[]);

} // namespace neuron::ncon

#pragma once

#include <filesystem>

namespace neuron::ncon {

int runContainerCommand(const std::filesystem::path &inputPath,
                        const char *invokerPath);

int runContainerDirect(const std::filesystem::path &inputPath);
int runWatchSessionDirect(const std::filesystem::path &sessionDir,
                          const std::filesystem::path &inputPath);

} // namespace neuron::ncon

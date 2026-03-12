#pragma once

#include "neuronc/ncon/Format.h"
#include "neuronc/ncon/RuntimeBridge.h"
#include "neuronc/ncon/Sandbox.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace neuron::ncon {

struct HotReloadCommand {
  std::filesystem::path containerPath;
  std::uint64_t sequence = 0;
};

struct HotReloadResult {
  enum class Status {
    Applied,
    RestartRequired,
    Failed,
  };

  Status status = Status::Failed;
  std::string message;
};

class VM {
public:
  VM(const ContainerData &container, const SandboxContext &sandbox);

  bool run(std::string *outError);
  bool runHotReloadSession(
      const std::function<std::optional<HotReloadCommand>()> &poller,
      const std::function<void(const HotReloadCommand &, const HotReloadResult &)>
          &reporter,
      std::string *outError);

private:
  const ContainerData &m_container;
  const SandboxContext &m_sandbox;
  RuntimeBridge m_runtime;
};

} // namespace neuron::ncon

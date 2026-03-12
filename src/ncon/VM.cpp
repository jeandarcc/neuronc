#include "VMInternal.h"

namespace neuron::ncon {

VM::VM(const ContainerData &container, const SandboxContext &sandbox)
    : m_container(container), m_sandbox(sandbox), m_runtime(&sandbox) {}

bool VM::run(std::string *outError) {
  detail::Executor executor(m_container, m_sandbox, &m_runtime);
  return executor.run(outError);
}

bool VM::runHotReloadSession(
    const std::function<std::optional<HotReloadCommand>()> &poller,
    const std::function<void(const HotReloadCommand &, const HotReloadResult &)>
        &reporter,
    std::string *outError) {
  detail::Executor executor(m_container, m_sandbox, &m_runtime);
  return executor.runHotReloadSession(poller, reporter, outError);
}

} // namespace neuron::ncon

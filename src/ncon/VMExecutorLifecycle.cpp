#include "VMInternal.h"

namespace neuron::ncon::detail {

Executor::Executor(const ContainerData &container, const SandboxContext &sandbox,
                   RuntimeBridge *runtime)
    : m_container(container), m_sandbox(sandbox), m_runtime(runtime) {
  rebuildFunctionMap();
}

bool Executor::run(std::string *outError) {
  if (m_runtime == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null runtime bridge";
    }
    return false;
  }

  initializeGlobals();
  if (!m_runtime->startup(m_container, m_container.program.moduleName, outError)) {
    return false;
  }
  const ExecutionStatus status =
      executeFunction(m_container.program.entryFunctionId, {}, nullptr, nullptr,
                      outError);
  m_runtime->shutdown();
  (void)m_sandbox;
  return status == ExecutionStatus::Ok;
}

bool Executor::runHotReloadSession(
    const std::function<std::optional<HotReloadCommand>()> &poller,
    const std::function<void(const HotReloadCommand &, const HotReloadResult &)>
        &reporter,
    std::string *outError) {
  if (m_runtime == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null runtime bridge";
    }
    return false;
  }

  initializeGlobals();
  if (!m_runtime->startup(m_container, m_container.program.moduleName, outError)) {
    return false;
  }

  std::string nextEntry = entryFunctionName();
  while (true) {
    if (nextEntry.empty()) {
      while (!m_pendingPatch.has_value()) {
        if (poller) {
          m_pendingPatch = poller();
        }
        if (!m_pendingPatch.has_value()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      }
    } else {
      const auto fnIt = m_functionIdsByName.find(nextEntry);
      if (fnIt == m_functionIdsByName.end()) {
        if (outError != nullptr) {
          *outError = "ncon hot reload hook not found: " + nextEntry;
        }
        m_runtime->shutdown();
        return false;
      }

      const ExecutionStatus status =
          executeFunction(fnIt->second, {}, nullptr, &poller, outError);
      if (status == ExecutionStatus::Failed) {
        m_runtime->shutdown();
        return false;
      }
      if (status == ExecutionStatus::Ok) {
        nextEntry.clear();
        continue;
      }
    }

    HotReloadResult result;
    std::string hookFunction;
    if (!applyPendingPatch(&result, &hookFunction, outError)) {
      result.status = HotReloadResult::Status::Failed;
      result.message =
          outError != nullptr && !outError->empty() ? *outError
                                                    : "failed to apply hot reload";
      if (m_pendingPatch.has_value() && reporter) {
        reporter(*m_pendingPatch, result);
      }
      m_runtime->shutdown();
      return false;
    }

    const HotReloadCommand command = *m_pendingPatch;
    m_pendingPatch.reset();
    if (reporter) {
      reporter(command, result);
    }
    if (result.status == HotReloadResult::Status::Applied) {
      nextEntry = hookFunction;
    } else {
      nextEntry.clear();
    }
  }
}

const Program &Executor::program() const { return m_container.program; }

std::string Executor::entryFunctionName() const {
  if (program().entryFunctionId == kInvalidIndex ||
      program().entryFunctionId >= program().functions.size()) {
    return "Init";
  }
  return stringAt(program().functions[program().entryFunctionId].nameStringId);
}

void Executor::rebuildFunctionMap() {
  m_functionIdsByName.clear();
  for (uint32_t i = 0; i < m_container.program.functions.size(); ++i) {
    m_functionIdsByName[stringAt(m_container.program.functions[i].nameStringId)] = i;
  }
}

void Executor::initializeGlobals() {
  m_globals.clear();
  m_globals.resize(program().globals.size());
  for (size_t i = 0; i < program().globals.size(); ++i) {
    const GlobalRecord &global = program().globals[i];
    auto cell = std::make_shared<Cell>();
    cell->typeId = global.typeId;
    cell->value = global.initializerConstantId == kInvalidIndex
                      ? defaultValue(global.typeId)
                      : coerce(constantValue(global.initializerConstantId),
                               global.typeId);
    m_globals[i] = cell;
  }
}

} // namespace neuron::ncon::detail


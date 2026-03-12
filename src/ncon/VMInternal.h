#pragma once

#include "neuronc/ncon/VM.h"

#include "neuronc/ncon/Manifest.h"
#include "neuron_tensor.h"

#include <chrono>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace neuron::ncon {

namespace detail {

enum class ExecutionStatus {
  Ok,
  Failed,
  PatchRequested,
};

struct HotReloadAnalysis {
  bool compatible = false;
  std::string reason;
  std::unordered_map<uint32_t, uint32_t> typeRemap;
  std::string hookFunction = "Init";
};

std::string canonicalTypeKey(const Program &program, uint32_t typeId,
                             std::unordered_map<uint32_t, std::string> *cache);

bool manifestsCompatible(const ContainerData &before, const ContainerData &after,
                         std::string *outReason);

bool analyzeHotReloadCompatibility(const ContainerData &before,
                                   const ContainerData &after,
                                   HotReloadAnalysis *outAnalysis);

class Executor {
public:
  Executor(const ContainerData &container, const SandboxContext &sandbox,
           RuntimeBridge *runtime);

  bool run(std::string *outError);
  bool runHotReloadSession(
      const std::function<std::optional<HotReloadCommand>()> &poller,
      const std::function<void(const HotReloadCommand &, const HotReloadResult &)>
          &reporter,
      std::string *outError);

private:
  struct Frame {
    uint32_t functionId = kInvalidIndex;
    std::vector<VMValue> slots;
  };

  const Program &program() const;
  const std::string &stringAt(uint32_t id) const;
  const TypeRecord *typeAt(uint32_t id) const;
  VMValue defaultValue(uint32_t typeId) const;
  VMValue constantValue(uint32_t constantId) const;
  int64_t toInt(const VMValue &value) const;
  double toDouble(const VMValue &value) const;
  bool isStringValue(const VMValue &value) const;
  bool isFloatingValue(const VMValue &value) const;
  std::string toString(const VMValue &value) const;
  PointerHandle toPointer(const VMValue &value) const;
  TensorHandle toTensor(const VMValue &value) const;
  bool truthy(const VMValue &value) const;
  static bool tryParseInt(const std::string &text, int64_t *outValue);
  static bool tryParseDouble(const std::string &text, double *outValue);
  bool tryCastValue(const VMValue &value, uint32_t typeId, VMValue *outValue,
                    std::string *outError) const;
  VMValue coerce(const VMValue &value, uint32_t typeId) const;
  VMValue operandValue(const Frame &frame, const OperandRecord &operand) const;
  const OperandRecord &operandAt(const InstructionRecord &instruction,
                                 uint32_t index) const;
  ExecutionStatus executeFunction(
      uint32_t functionId, const std::vector<VMValue> &args, VMValue *outValue,
      const std::function<std::optional<HotReloadCommand>()> *poller,
      std::string *outError);
  bool executeTensorInstruction(const Frame &frame,
                                const InstructionRecord &instruction,
                                Opcode opcode, VMValue *outValue,
                                std::string *outError);
  std::string entryFunctionName() const;
  void rebuildFunctionMap();
  void remapCellTypes(const PointerHandle &cell,
                      const std::unordered_map<uint32_t, uint32_t> &typeRemap,
                      std::unordered_set<const Cell *> *visitedCells,
                      std::unordered_set<const ClassObject *> *visitedObjects);
  void remapObjectTypes(
      const ClassObjectHandle &object,
      const std::unordered_map<uint32_t, uint32_t> &typeRemap,
      std::unordered_set<const Cell *> *visitedCells,
      std::unordered_set<const ClassObject *> *visitedObjects);
  void remapValueTypes(const VMValue *value,
                       const std::unordered_map<uint32_t, uint32_t> &typeRemap,
                       std::unordered_set<const Cell *> *visitedCells,
                       std::unordered_set<const ClassObject *> *visitedObjects);
  bool applyPendingPatch(HotReloadResult *outResult, std::string *outHookFunction,
                         std::string *outError);
  void initializeGlobals();

  ContainerData m_container;
  const SandboxContext &m_sandbox;
  RuntimeBridge *m_runtime = nullptr;
  std::unordered_map<std::string, uint32_t> m_functionIdsByName;
  std::vector<PointerHandle> m_globals;
  std::optional<HotReloadCommand> m_pendingPatch;
};

} // namespace detail

} // namespace neuron::ncon


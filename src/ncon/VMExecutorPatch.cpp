#include "VMInternal.h"

namespace neuron::ncon::detail {

void Executor::remapCellTypes(
    const PointerHandle &cell,
    const std::unordered_map<uint32_t, uint32_t> &typeRemap,
    std::unordered_set<const Cell *> *visitedCells,
    std::unordered_set<const ClassObject *> *visitedObjects) {
  if (!cell || visitedCells == nullptr || !visitedCells->insert(cell.get()).second) {
    return;
  }

  auto typeIt = typeRemap.find(cell->typeId);
  if (typeIt != typeRemap.end()) {
    cell->typeId = typeIt->second;
  }
  remapValueTypes(&cell->value, typeRemap, visitedCells, visitedObjects);
}

void Executor::remapObjectTypes(
    const ClassObjectHandle &object,
    const std::unordered_map<uint32_t, uint32_t> &typeRemap,
    std::unordered_set<const Cell *> *visitedCells,
    std::unordered_set<const ClassObject *> *visitedObjects) {
  if (!object || visitedObjects == nullptr ||
      !visitedObjects->insert(object.get()).second) {
    return;
  }
  for (const PointerHandle &field : object->fields) {
    remapCellTypes(field, typeRemap, visitedCells, visitedObjects);
  }
}

void Executor::remapValueTypes(
    const VMValue *value,
    const std::unordered_map<uint32_t, uint32_t> &typeRemap,
    std::unordered_set<const Cell *> *visitedCells,
    std::unordered_set<const ClassObject *> *visitedObjects) {
  if (value == nullptr) {
    return;
  }
  if (const auto *cell = std::get_if<PointerHandle>(&value->data)) {
    remapCellTypes(*cell, typeRemap, visitedCells, visitedObjects);
    return;
  }
  if (const auto *object = std::get_if<ClassObjectHandle>(&value->data)) {
    remapObjectTypes(*object, typeRemap, visitedCells, visitedObjects);
  }
}

bool Executor::applyPendingPatch(HotReloadResult *outResult,
                                 std::string *outHookFunction,
                                 std::string *outError) {
  if (outResult == nullptr || outHookFunction == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null hot reload output";
    }
    return false;
  }
  if (!m_pendingPatch.has_value()) {
    outResult->status = HotReloadResult::Status::Failed;
    outResult->message = "no pending hot reload request";
    if (outError != nullptr) {
      *outError = outResult->message;
    }
    return false;
  }

  ContainerData nextContainer;
  if (!readContainer(m_pendingPatch->containerPath, &nextContainer, outError)) {
    return false;
  }

  HotReloadAnalysis analysis;
  if (!analyzeHotReloadCompatibility(m_container, nextContainer, &analysis)) {
    if (outError != nullptr) {
      *outError = "failed to analyze hot reload compatibility";
    }
    return false;
  }
  if (!analysis.compatible) {
    outResult->status = HotReloadResult::Status::RestartRequired;
    outResult->message =
        analysis.reason.empty() ? "hot reload requires restart" : analysis.reason;
    return true;
  }

  m_container = std::move(nextContainer);
  rebuildFunctionMap();
  for (size_t i = 0; i < m_globals.size() && i < program().globals.size(); ++i) {
    if (!m_globals[i]) {
      continue;
    }
    m_globals[i]->typeId = program().globals[i].typeId;
  }

  std::unordered_set<const Cell *> visitedCells;
  std::unordered_set<const ClassObject *> visitedObjects;
  for (const PointerHandle &global : m_globals) {
    remapCellTypes(global, analysis.typeRemap, &visitedCells, &visitedObjects);
  }

  outResult->status = HotReloadResult::Status::Applied;
  outResult->message = "applied";
  *outHookFunction = analysis.hookFunction;
  return true;
}

} // namespace neuron::ncon::detail


#include "neuronc/mir/MIROwnershipPass.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace neuron::mir {

namespace {

enum class ValueKind {
  Unknown,
  Temporary,
  ObservedOwner,
  BorrowedRef,
  MovedOwner,
};

enum class VariableKind {
  Unknown,
  Owned,
  Borrowed,
  Moved,
};

struct ValueState {
  ValueKind kind = ValueKind::Unknown;
  std::string source;
  std::string owner;
};

struct VariableState {
  VariableKind kind = VariableKind::Unknown;
  std::string owner;
};

std::string bindingMode(std::string_view note) {
  constexpr std::string_view prefix = "binding:";
  if (note.rfind(prefix, 0) == 0) {
    return std::string(note.substr(prefix.size()));
  }
  return "value";
}

class FunctionOwnershipAnalyzer {
public:
  FunctionOwnershipAnalyzer(const Function &function,
                            sema_detail::DiagnosticEmitter &diagnostics,
                            std::vector<OwnershipHint> &hints)
      : m_function(function), m_diagnostics(diagnostics), m_hints(hints) {
    for (const auto &local : m_function.locals) {
      m_locals[local.name] = &local;
    }
    for (const std::string &parameter : m_function.parameters) {
      m_variables[parameter] = {VariableKind::Owned, ""};
      noteOwner(parameter);
    }
  }

  void run() {
    for (const auto &block : m_function.blocks) {
      for (const auto &inst : block.instructions) {
        visit(inst);
      }
    }
  }

private:
  const Function &m_function;
  sema_detail::DiagnosticEmitter &m_diagnostics;
  std::vector<OwnershipHint> &m_hints;
  std::unordered_map<std::string, const LocalInfo *> m_locals;
  std::unordered_map<std::string, VariableState> m_variables;
  std::unordered_map<std::string, ValueState> m_temps;
  std::unordered_map<std::string, std::unordered_set<std::string>> m_borrows;
  std::unordered_map<std::string, std::size_t> m_hintIndex;

  const LocalInfo *findLocal(const std::string &name) const {
    const auto it = m_locals.find(name);
    return it == m_locals.end() ? nullptr : it->second;
  }

  std::string displayName(const std::string &name) const {
    const LocalInfo *local = findLocal(name);
    return local != nullptr && !local->sourceName.empty() ? local->sourceName : name;
  }

  void emit(const SourceLocation &location, std::string message) {
    m_diagnostics.emit(location, std::move(message));
  }

  void upsertHint(const std::string &name, std::string label, std::string tooltip) {
    const LocalInfo *local = findLocal(name);
    if (local == nullptr || local->location.file.empty()) {
      return;
    }
    OwnershipHint hint;
    hint.location = local->location;
    hint.length = std::max(1, static_cast<int>(displayName(name).size()));
    hint.label = std::move(label);
    hint.tooltip = std::move(tooltip);
    const auto [it, inserted] = m_hintIndex.emplace(name, m_hints.size());
    if (inserted) {
      m_hints.push_back(std::move(hint));
      return;
    }
    m_hints[it->second] = std::move(hint);
  }

  void noteOwner(const std::string &name) {
    upsertHint(name, " owner",
               "Single-owner value. Use 'move' to transfer ownership.");
  }

  void noteBorrow(const std::string &name, const std::string &owner) {
    upsertHint(name, " ref " + displayName(owner),
               "Borrowed reference to '" + displayName(owner) +
                   "'. The reference must not outlive its owner.");
  }

  void releaseBorrow(const std::string &name) {
    const auto it = m_variables.find(name);
    if (it == m_variables.end() || it->second.kind != VariableKind::Borrowed ||
        it->second.owner.empty()) {
      return;
    }
    auto ownerIt = m_borrows.find(it->second.owner);
    if (ownerIt != m_borrows.end()) {
      ownerIt->second.erase(name);
      if (ownerIt->second.empty()) {
        m_borrows.erase(ownerIt);
      }
    }
  }

  bool borrowedOwnerWouldOutlive(const std::string &target,
                                 const std::string &owner) const {
    const LocalInfo *targetLocal = findLocal(target);
    const LocalInfo *ownerLocal = findLocal(owner);
    return targetLocal != nullptr && ownerLocal != nullptr &&
           targetLocal->scopeDepth < ownerLocal->scopeDepth;
  }

  ValueState materialize(const Operand &operand, const SourceLocation &location) {
    if (operand.kind == OperandKind::Temp) {
      const auto it = m_temps.find(operand.text);
      return it == m_temps.end() ? ValueState{ValueKind::Temporary, "", ""}
                                 : it->second;
    }
    if (operand.kind != OperandKind::Variable) {
      return {ValueKind::Temporary, "", ""};
    }

    const auto it = m_variables.find(operand.text);
    if (it == m_variables.end()) {
      return {ValueKind::Unknown, operand.text, operand.text};
    }
    if (it->second.kind == VariableKind::Moved) {
      emit(location, "Ownership violation: '" + displayName(operand.text) +
                         "' was used after ownership was moved.");
      return {ValueKind::Unknown, operand.text, operand.text};
    }
    if (it->second.kind == VariableKind::Borrowed) {
      return {ValueKind::BorrowedRef, operand.text, it->second.owner};
    }
    return {ValueKind::ObservedOwner, operand.text, operand.text};
  }

  void requireNoActiveBorrows(const std::string &owner,
                              const SourceLocation &location,
                              std::string_view action) {
    const auto it = m_borrows.find(owner);
    if (it == m_borrows.end() || it->second.empty()) {
      return;
    }
    emit(location, "Ownership violation: cannot " + std::string(action) +
                       " owner '" + displayName(owner) +
                       "' while '" + displayName(*it->second.begin()) +
                       "' still borrows it.");
  }

  void setBorrowed(const std::string &target, const std::string &owner,
                   const SourceLocation &location) {
    requireNoActiveBorrows(target, location, "overwrite");
    releaseBorrow(target);
    m_variables[target] = {VariableKind::Borrowed, owner};
    m_borrows[owner].insert(target);
    noteBorrow(target, owner);
    if (borrowedOwnerWouldOutlive(target, owner)) {
      emit(location, "Lifetime violation: reference '" + displayName(target) +
                         "' may outlive owner '" + displayName(owner) + "'.");
    }
  }

  void setOwned(const std::string &target, const SourceLocation &location) {
    requireNoActiveBorrows(target, location, "overwrite");
    releaseBorrow(target);
    m_variables[target] = {VariableKind::Owned, ""};
    noteOwner(target);
  }

  void visit(const Instruction &inst) {
    switch (inst.kind) {
    case InstKind::Constant:
      m_temps[inst.result] = {ValueKind::Temporary, "", ""};
      return;
    case InstKind::Copy:
      m_temps[inst.result] = materialize(inst.operands.front(), inst.location);
      return;
    case InstKind::Move: {
      const std::string &source = inst.operands.front().text;
      ValueState observed = materialize(inst.operands.front(), inst.location);
      if (observed.kind == ValueKind::BorrowedRef) {
        emit(inst.location, "Ownership violation: cannot move borrowed reference '" +
                               displayName(source) + "'.");
        m_temps[inst.result] = {ValueKind::Unknown, source, source};
        return;
      }
      requireNoActiveBorrows(source, inst.location, "move");
      m_variables[source] = {VariableKind::Moved, ""};
      m_temps[inst.result] = {ValueKind::MovedOwner, source, source};
      return;
    }
    case InstKind::Borrow: {
      ValueState source = materialize(inst.operands.front(), inst.location);
      const std::string owner =
          !source.owner.empty() ? source.owner : inst.operands.front().text;
      m_temps[inst.result] = {ValueKind::BorrowedRef, inst.operands.front().text,
                              owner};
      return;
    }
    case InstKind::Deref:
    case InstKind::Unary:
    case InstKind::Binary:
    case InstKind::Call:
    case InstKind::Member:
    case InstKind::Index:
    case InstKind::Slice:
    case InstKind::Typeof:
    case InstKind::Cast:
      m_temps[inst.result] = {ValueKind::Temporary, "", ""};
      return;
    case InstKind::Bind:
    case InstKind::Assign:
      applyAssignment(inst);
      return;
    case InstKind::Return:
      if (!inst.operands.empty()) {
        ValueState result = materialize(inst.operands.front(), inst.location);
        if (result.kind == ValueKind::BorrowedRef && !result.owner.empty()) {
          emit(inst.location,
               "Lifetime violation: cannot return borrowed reference to '" +
                   displayName(result.owner) + "'.");
        }
      }
      return;
    default:
      return;
    }
  }

  void applyAssignment(const Instruction &inst) {
    if (inst.result.empty() || inst.operands.empty()) {
      return;
    }

    const std::string mode = bindingMode(inst.note);
    const std::string &target = inst.result;
    ValueState value = materialize(inst.operands.front(), inst.location);

    if (mode == "address_of") {
      releaseBorrow(target);
      if (value.kind != ValueKind::BorrowedRef || value.owner.empty()) {
        emit(inst.location,
             "Ownership violation: cannot create a reference without a live owner.");
        m_variables[target] = {VariableKind::Unknown, ""};
        return;
      }
      setBorrowed(target, value.owner, inst.location);
      return;
    }

    if (value.kind == ValueKind::BorrowedRef && !value.owner.empty()) {
      releaseBorrow(target);
      setBorrowed(target, value.owner, inst.location);
      return;
    }

    if (value.kind == ValueKind::ObservedOwner && !value.source.empty() &&
        value.source != target && mode != "move") {
      emit(inst.location,
           "Ownership violation: binding '" + displayName(target) + "' from '" +
               displayName(value.source) +
               "' would create a second owner. Use 'move' to transfer ownership.");
    }

    setOwned(target, inst.location);
  }
};

} // namespace

void MIROwnershipPass::reset() {
  m_diagnostics.reset();
  m_hints.clear();
}

void MIROwnershipPass::setSourceText(std::string sourceText) {
  m_diagnostics.setSourceText(std::move(sourceText));
}

void MIROwnershipPass::setAgentHints(bool enabled) {
  m_diagnostics.setAgentHints(enabled);
}

void MIROwnershipPass::run(const Module &module) {
  reset();
  for (const auto &function : module.functions) {
    FunctionOwnershipAnalyzer(function, m_diagnostics, m_hints).run();
  }
  std::sort(m_hints.begin(), m_hints.end(),
            [](const OwnershipHint &lhs, const OwnershipHint &rhs) {
              if (lhs.location.file != rhs.location.file) {
                return lhs.location.file < rhs.location.file;
              }
              if (lhs.location.line != rhs.location.line) {
                return lhs.location.line < rhs.location.line;
              }
              return lhs.location.column < rhs.location.column;
            });
}

const std::vector<SemanticError> &MIROwnershipPass::errors() const {
  return m_diagnostics.errors();
}

const std::vector<OwnershipHint> &MIROwnershipPass::hints() const {
  return m_hints;
}

bool MIROwnershipPass::hasErrors() const { return m_diagnostics.hasErrors(); }

} // namespace neuron::mir

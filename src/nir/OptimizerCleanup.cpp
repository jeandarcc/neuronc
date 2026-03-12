#include "OptimizerInternal.h"

#include <unordered_set>

namespace neuron::nir {

using namespace detail;

bool DeadStoreEliminationPass::runOnModule(Module *module) {
  bool changed = false;

  for (const auto &func : module->getFunctions()) {
    for (const auto &block : func->getBlocks()) {
      auto &insts = block->getInstructionsMut();

      bool hasCall = false;
      for (const auto &instPtr : insts) {
        if (instPtr && instPtr->getKind() == InstKind::Call) {
          hasCall = true;
          break;
        }
      }
      if (hasCall) {
        continue;
      }

      std::unordered_set<Value *> livePointers;
      for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(insts.size()) - 1; i >= 0;
           --i) {
        Instruction *inst = insts[static_cast<size_t>(i)].get();
        if (inst == nullptr) {
          continue;
        }

        if (inst->getKind() == InstKind::Load && inst->getOperands().size() >= 1) {
          livePointers.insert(inst->getOperand(0));
          continue;
        }

        if (inst->getKind() == InstKind::Store && inst->getOperands().size() >= 2) {
          Value *ptr = inst->getOperand(1);
          if (!isAlloca(ptr)) {
            continue;
          }
          if (livePointers.find(ptr) == livePointers.end()) {
            insts.erase(insts.begin() + i);
            changed = true;
            continue;
          }
          livePointers.erase(ptr);
        }
      }
    }
  }

  return changed;
}

bool BranchSimplificationPass::runOnModule(Module *module) {
  bool changed = false;

  for (const auto &func : module->getFunctions()) {
    for (const auto &block : func->getBlocks()) {
      auto &insts = block->getInstructionsMut();
      for (auto &instPtr : insts) {
        Instruction *inst = instPtr.get();
        if (inst == nullptr || inst->getKind() != InstKind::CondBr ||
            inst->getOperands().size() < 3) {
          continue;
        }

        bool chooseThen = false;
        bool canSimplify = false;
        if (auto *ci = dynamic_cast<ConstantInt *>(inst->getOperand(0))) {
          chooseThen = (ci->getValue() != 0);
          canSimplify = true;
        } else if (auto *cf = dynamic_cast<ConstantFloat *>(inst->getOperand(0))) {
          chooseThen = (cf->getValue() != 0.0);
          canSimplify = true;
        }

        if (!canSimplify) {
          continue;
        }

        Value *target = inst->getOperand(chooseThen ? 1 : 2);
        auto replacement = std::make_unique<Instruction>(InstKind::Br,
                                                         NType::makeVoid(),
                                                         inst->getName());
        replacement->addOperand(target);
        instPtr = std::move(replacement);
        changed = true;
      }
    }
  }

  return changed;
}

bool DeadCodeEliminationPass::runOnModule(Module *module) {
  bool changed = false;
  std::unordered_set<Value *> usedValues;

  for (const auto &func : module->getFunctions()) {
    for (const auto &block : func->getBlocks()) {
      for (const auto &instPtr : block->getInstructions()) {
        Instruction *inst = instPtr.get();
        if (inst == nullptr) {
          continue;
        }
        for (Value *op : inst->getOperands()) {
          usedValues.insert(op);
        }
      }
    }
  }

  for (const auto &func : module->getFunctions()) {
    for (const auto &block : func->getBlocks()) {
      auto &insts = block->getInstructionsMut();
      auto it = insts.begin();
      while (it != insts.end()) {
        Instruction *inst = it->get();
        if (!inst) {
          it = insts.erase(it);
          continue;
        }

        bool hasSideEffects = false;
        InstKind kind = inst->getKind();
        if (kind == InstKind::Store || kind == InstKind::Call ||
            kind == InstKind::Ret || kind == InstKind::Br ||
            kind == InstKind::CondBr || kind == InstKind::Alloca ||
            kind == InstKind::GpuScopeBegin ||
            kind == InstKind::GpuScopeEnd ||
            kind == InstKind::TensorMatMul ||
            kind == InstKind::TensorMatMulAdd ||
            kind == InstKind::TensorLinearFused ||
            kind == InstKind::TensorFMA) {
          hasSideEffects = true;
        }

        if (!hasSideEffects && usedValues.find(inst) == usedValues.end()) {
          it = insts.erase(it);
          changed = true;
        } else {
          ++it;
        }
      }
    }
  }
  return changed;
}

bool DeadCodeEliminationPass::elideDeadCode(Block *block) {
  (void)block;
  return false;
}

} // namespace neuron::nir


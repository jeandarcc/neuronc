#include "OptimizerInternal.h"

#include <unordered_set>

namespace neuron::nir {

bool TensorFusionPass::runOnModule(Module *module) {
  bool changed = false;
  std::unordered_set<Instruction *> eraseCandidates;

  auto replaceAllUses = [&](Function *fn, Value *from, Value *to) {
    if (fn == nullptr || from == nullptr || to == nullptr || from == to) {
      return;
    }
    for (const auto &useBlock : fn->getBlocks()) {
      for (const auto &useInstPtr : useBlock->getInstructions()) {
        Instruction *useInst = useInstPtr.get();
        if (useInst == nullptr) {
          continue;
        }
        for (size_t opIndex = 0; opIndex < useInst->getOperands().size(); ++opIndex) {
          if (useInst->getOperand(opIndex) == from) {
            useInst->setOperand(opIndex, to);
          }
        }
      }
    }
  };

  for (const auto &func : module->getFunctions()) {
    for (const auto &block : func->getBlocks()) {
      auto &insts = block->getInstructionsMut();
      for (auto &instPtr : insts) {
        Instruction *inst = instPtr.get();
        if (!inst) {
          continue;
        }

        if (inst->getKind() == InstKind::TensorAdd) {
          Value *lhs = inst->getOperand(0);
          Value *rhs = inst->getOperand(1);

          auto *lhsInst = dynamic_cast<Instruction *>(lhs);
          auto *rhsInst = dynamic_cast<Instruction *>(rhs);
          const ExecutionHint baseHint = inst->getExecutionHint();

          if (lhsInst && lhsInst->getKind() == InstKind::TensorMul &&
              lhsInst->getExecutionHint() == baseHint) {
            Instruction *oldInst = inst;
            auto fmaOwned = std::make_unique<Instruction>(InstKind::TensorFMA,
                                                          inst->getType(),
                                                          inst->getName());
            Instruction *fma = fmaOwned.get();
            fma->setExecutionHint(baseHint);
            fma->addOperand(lhsInst->getOperand(0));
            fma->addOperand(lhsInst->getOperand(1));
            fma->addOperand(rhs);
            instPtr = std::move(fmaOwned);
            replaceAllUses(func.get(), oldInst, fma);
            eraseCandidates.insert(lhsInst);
            changed = true;
          } else if (rhsInst && rhsInst->getKind() == InstKind::TensorMul &&
                     rhsInst->getExecutionHint() == baseHint) {
            Instruction *oldInst = inst;
            auto fmaOwned = std::make_unique<Instruction>(InstKind::TensorFMA,
                                                          inst->getType(),
                                                          inst->getName());
            Instruction *fma = fmaOwned.get();
            fma->setExecutionHint(baseHint);
            fma->addOperand(rhsInst->getOperand(0));
            fma->addOperand(rhsInst->getOperand(1));
            fma->addOperand(lhs);
            instPtr = std::move(fmaOwned);
            replaceAllUses(func.get(), oldInst, fma);
            eraseCandidates.insert(rhsInst);
            changed = true;
          } else if (lhsInst && lhsInst->getKind() == InstKind::TensorMatMul &&
                     lhsInst->getExecutionHint() == baseHint) {
            Instruction *oldInst = inst;
            auto fusedOwned = std::make_unique<Instruction>(
                InstKind::TensorMatMulAdd, inst->getType(), inst->getName());
            Instruction *fused = fusedOwned.get();
            fused->setExecutionHint(baseHint);
            fused->addOperand(lhsInst->getOperand(0));
            fused->addOperand(lhsInst->getOperand(1));
            fused->addOperand(rhs);
            instPtr = std::move(fusedOwned);
            replaceAllUses(func.get(), oldInst, fused);
            eraseCandidates.insert(lhsInst);
            changed = true;
          } else if (rhsInst && rhsInst->getKind() == InstKind::TensorMatMul &&
                     rhsInst->getExecutionHint() == baseHint) {
            Instruction *oldInst = inst;
            auto fusedOwned = std::make_unique<Instruction>(
                InstKind::TensorMatMulAdd, inst->getType(), inst->getName());
            Instruction *fused = fusedOwned.get();
            fused->setExecutionHint(baseHint);
            fused->addOperand(rhsInst->getOperand(0));
            fused->addOperand(rhsInst->getOperand(1));
            fused->addOperand(lhs);
            instPtr = std::move(fusedOwned);
            replaceAllUses(func.get(), oldInst, fused);
            eraseCandidates.insert(rhsInst);
            changed = true;
          } else if (lhsInst &&
                     lhsInst->getKind() == InstKind::TensorMatMulAdd &&
                     lhsInst->getExecutionHint() == baseHint) {
            Instruction *oldInst = inst;
            auto fusedOwned = std::make_unique<Instruction>(
                InstKind::TensorLinearFused, inst->getType(), inst->getName());
            Instruction *fused = fusedOwned.get();
            fused->setExecutionHint(baseHint);
            fused->addOperand(lhsInst->getOperand(0));
            fused->addOperand(lhsInst->getOperand(1));
            fused->addOperand(lhsInst->getOperand(2));
            fused->addOperand(rhs);
            instPtr = std::move(fusedOwned);
            replaceAllUses(func.get(), oldInst, fused);
            eraseCandidates.insert(lhsInst);
            changed = true;
          } else if (rhsInst &&
                     rhsInst->getKind() == InstKind::TensorMatMulAdd &&
                     rhsInst->getExecutionHint() == baseHint) {
            Instruction *oldInst = inst;
            auto fusedOwned = std::make_unique<Instruction>(
                InstKind::TensorLinearFused, inst->getType(), inst->getName());
            Instruction *fused = fusedOwned.get();
            fused->setExecutionHint(baseHint);
            fused->addOperand(rhsInst->getOperand(0));
            fused->addOperand(rhsInst->getOperand(1));
            fused->addOperand(rhsInst->getOperand(2));
            fused->addOperand(lhs);
            instPtr = std::move(fusedOwned);
            replaceAllUses(func.get(), oldInst, fused);
            eraseCandidates.insert(rhsInst);
            changed = true;
          }
        }
      }
    }
  }

  if (!eraseCandidates.empty()) {
    for (const auto &func : module->getFunctions()) {
      auto isUsedInFunction = [&](Instruction *target) {
        for (const auto &useBlock : func->getBlocks()) {
          for (const auto &useInstPtr : useBlock->getInstructions()) {
            const Instruction *useInst = useInstPtr.get();
            if (useInst == nullptr || useInst == target) {
              continue;
            }
            for (Value *operand : useInst->getOperands()) {
              if (operand == target) {
                return true;
              }
            }
          }
        }
        return false;
      };

      for (const auto &block : func->getBlocks()) {
        auto &insts = block->getInstructionsMut();
        auto it = insts.begin();
        while (it != insts.end()) {
          Instruction *candidate = it->get();
          if (candidate != nullptr &&
              eraseCandidates.find(candidate) != eraseCandidates.end() &&
              !isUsedInFunction(candidate)) {
            it = insts.erase(it);
            changed = true;
          } else {
            ++it;
          }
        }
      }
    }
  }

  return changed;
}

} // namespace neuron::nir


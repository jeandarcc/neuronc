#include "OptimizerInternal.h"

#include <unordered_map>

namespace neuron::nir {

using namespace detail;

Value *ConstantFoldingPass::makeIntConstant(int64_t value) {
  m_ownedConstants.push_back(std::make_unique<ConstantInt>(value));
  return m_ownedConstants.back().get();
}

Value *ConstantFoldingPass::makeFloatConstant(double value) {
  m_ownedConstants.push_back(std::make_unique<ConstantFloat>(value));
  return m_ownedConstants.back().get();
}

bool ConstantFoldingPass::runOnModule(Module *module) {
  bool changed = false;
  m_ownedConstants.clear();
  for (const auto &func : module->getFunctions()) {
    for (const auto &block : func->getBlocks()) {
      if (foldBlock(block.get(), module)) {
        changed = true;
      }
    }
  }
  return changed;
}

bool ConstantFoldingPass::foldBlock(Block *block, Module *module) {
  (void)module;
  bool changed = false;
  std::unordered_map<Value *, Value *> replacements;

  for (const auto &instPtr : block->getInstructions()) {
    Instruction *inst = instPtr.get();
    if (inst == nullptr) {
      continue;
    }

    replaceOperands(inst, replacements, &changed);

    if (inst->getOperands().size() != 2) {
      continue;
    }

    Value *lhsVal = inst->getOperand(0);
    Value *rhsVal = inst->getOperand(1);

    if (auto *lhs = dynamic_cast<ConstantInt *>(lhsVal)) {
      auto *rhs = dynamic_cast<ConstantInt *>(rhsVal);
      if (rhs == nullptr) {
        continue;
      }

      Value *result = nullptr;
      switch (inst->getKind()) {
      case InstKind::Add:
        result = makeIntConstant(lhs->getValue() + rhs->getValue());
        break;
      case InstKind::Sub:
        result = makeIntConstant(lhs->getValue() - rhs->getValue());
        break;
      case InstKind::Mul:
        result = makeIntConstant(lhs->getValue() * rhs->getValue());
        break;
      case InstKind::Div:
        if (rhs->getValue() != 0) {
          result = makeIntConstant(lhs->getValue() / rhs->getValue());
        }
        break;
      default:
        break;
      }
      if (result != nullptr) {
        replacements[inst] = result;
      }
      continue;
    }

    if (auto *lhs = dynamic_cast<ConstantFloat *>(lhsVal)) {
      auto *rhs = dynamic_cast<ConstantFloat *>(rhsVal);
      if (rhs == nullptr) {
        continue;
      }

      Value *result = nullptr;
      switch (inst->getKind()) {
      case InstKind::FAdd:
        result = makeFloatConstant(lhs->getValue() + rhs->getValue());
        break;
      case InstKind::FSub:
        result = makeFloatConstant(lhs->getValue() - rhs->getValue());
        break;
      case InstKind::FMul:
        result = makeFloatConstant(lhs->getValue() * rhs->getValue());
        break;
      case InstKind::FDiv:
        if (rhs->getValue() != 0.0) {
          result = makeFloatConstant(lhs->getValue() / rhs->getValue());
        }
        break;
      default:
        break;
      }
      if (result != nullptr) {
        replacements[inst] = result;
      }
    }
  }

  return changed;
}

bool CopyPropagationPass::runOnModule(Module *module) {
  bool changed = false;

  for (const auto &func : module->getFunctions()) {
    for (const auto &block : func->getBlocks()) {
      std::unordered_map<Value *, Value *> replacements;
      std::unordered_map<Value *, Value *> storedValues;

      for (const auto &instPtr : block->getInstructions()) {
        Instruction *inst = instPtr.get();
        if (inst == nullptr) {
          continue;
        }

        replaceOperands(inst, replacements, &changed);

        if (inst->getKind() == InstKind::Store && inst->getOperands().size() >= 2) {
          Value *value = inst->getOperand(0);
          Value *ptr = inst->getOperand(1);
          storedValues[ptr] = value;
          continue;
        }

        if (inst->getKind() == InstKind::Load && inst->getOperands().size() >= 1) {
          Value *ptr = inst->getOperand(0);
          auto it = storedValues.find(ptr);
          if (it != storedValues.end() && it->second != nullptr) {
            replacements[inst] = it->second;
          }
          continue;
        }

        if (inst->getKind() == InstKind::Call) {
          storedValues.clear();
        }
      }
    }
  }

  return changed;
}

Value *AlgebraicSimplificationPass::makeIntConstant(int64_t value) {
  m_ownedConstants.push_back(std::make_unique<ConstantInt>(value));
  return m_ownedConstants.back().get();
}

Value *AlgebraicSimplificationPass::makeFloatConstant(double value) {
  m_ownedConstants.push_back(std::make_unique<ConstantFloat>(value));
  return m_ownedConstants.back().get();
}

bool AlgebraicSimplificationPass::runOnModule(Module *module) {
  bool changed = false;
  m_ownedConstants.clear();

  for (const auto &func : module->getFunctions()) {
    for (const auto &block : func->getBlocks()) {
      std::unordered_map<Value *, Value *> replacements;

      for (const auto &instPtr : block->getInstructions()) {
        Instruction *inst = instPtr.get();
        if (inst == nullptr) {
          continue;
        }

        replaceOperands(inst, replacements, &changed);
        if (inst->getOperands().size() != 2) {
          continue;
        }

        Value *lhs = inst->getOperand(0);
        Value *rhs = inst->getOperand(1);
        Value *replacement = nullptr;

        switch (inst->getKind()) {
        case InstKind::Add:
        case InstKind::FAdd:
          if (isZero(lhs)) {
            replacement = rhs;
          } else if (isZero(rhs)) {
            replacement = lhs;
          }
          break;
        case InstKind::Sub:
        case InstKind::FSub:
          if (isZero(rhs)) {
            replacement = lhs;
          }
          break;
        case InstKind::Mul:
        case InstKind::FMul:
          if (isOne(lhs)) {
            replacement = rhs;
          } else if (isOne(rhs)) {
            replacement = lhs;
          } else if (isZero(lhs) || isZero(rhs)) {
            if (inst->getKind() == InstKind::FMul) {
              replacement = makeFloatConstant(0.0);
            } else {
              replacement = makeIntConstant(0);
            }
          }
          break;
        case InstKind::Div:
          if (isOne(rhs)) {
            replacement = lhs;
          } else if (lhs == rhs) {
            replacement = makeIntConstant(1);
          }
          break;
        case InstKind::FDiv:
          if (isOne(rhs)) {
            replacement = lhs;
          }
          break;
        default:
          break;
        }

        if (replacement != nullptr) {
          replacements[inst] = replacement;
        }
      }
    }
  }

  return changed;
}

} // namespace neuron::nir

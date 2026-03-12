#include "OptimizerInternal.h"

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace neuron::nir {

namespace {

Instruction *getTerminator(Block *block) {
  if (block == nullptr) {
    return nullptr;
  }
  auto &insts = block->getInstructionsMut();
  if (insts.empty()) {
    return nullptr;
  }
  Instruction *last = insts.back().get();
  if (last == nullptr) {
    return nullptr;
  }
  const InstKind kind = last->getKind();
  if (kind == InstKind::Br || kind == InstKind::CondBr || kind == InstKind::Ret) {
    return last;
  }
  return nullptr;
}

Block *getBlockOperand(Instruction *inst, size_t operand_index) {
  if (inst == nullptr || inst->getOperands().size() <= operand_index) {
    return nullptr;
  }
  Value *op = inst->getOperand(operand_index);
  if (op == nullptr || op->getValueKind() != ValueKind::Block) {
    return nullptr;
  }
  return static_cast<BlockRef *>(op)->getBlock();
}

bool isTerminatorKind(InstKind kind) {
  return kind == InstKind::Br || kind == InstKind::CondBr || kind == InstKind::Ret;
}

} // namespace

bool GpuScopeLiftingPass::runOnModule(Module *module) {
  if (module == nullptr) {
    return false;
  }

  bool changed = false;
  for (const auto &func : module->getFunctions()) {
    std::unordered_map<Block *, std::vector<Block *>> predecessors;

    for (const auto &block_ptr : func->getBlocks()) {
      Block *block = block_ptr.get();
      Instruction *term = getTerminator(block);
      if (term == nullptr) {
        continue;
      }

      if (term->getKind() == InstKind::Br) {
        Block *target = getBlockOperand(term, 0);
        if (target != nullptr) {
          predecessors[target].push_back(block);
        }
      } else if (term->getKind() == InstKind::CondBr) {
        Block *then_block = getBlockOperand(term, 1);
        Block *else_block = getBlockOperand(term, 2);
        if (then_block != nullptr) {
          predecessors[then_block].push_back(block);
        }
        if (else_block != nullptr) {
          predecessors[else_block].push_back(block);
        }
      }
    }

    for (const auto &cond_ptr : func->getBlocks()) {
      Block *cond_block = cond_ptr.get();
      Instruction *cond_term = getTerminator(cond_block);
      if (cond_term == nullptr || cond_term->getKind() != InstKind::CondBr) {
        continue;
      }

      Block *body_block = getBlockOperand(cond_term, 1);
      Block *exit_block = getBlockOperand(cond_term, 2);
      if (body_block == nullptr || exit_block == nullptr) {
        continue;
      }

      Block *inc_block = nullptr;
      Block *backedge_block = nullptr;
      Instruction *body_term = getTerminator(body_block);
      if (body_term == nullptr || body_term->getKind() != InstKind::Br) {
        continue;
      }

      Block *body_target = getBlockOperand(body_term, 0);
      if (body_target == cond_block) {
        backedge_block = body_block;
      } else {
        inc_block = body_target;
        Instruction *inc_term = getTerminator(inc_block);
        if (inc_term == nullptr || inc_term->getKind() != InstKind::Br ||
            getBlockOperand(inc_term, 0) != cond_block) {
          continue;
        }
        backedge_block = inc_block;
      }

      auto pred_it = predecessors.find(cond_block);
      if (pred_it == predecessors.end()) {
        continue;
      }

      Block *preheader = nullptr;
      for (Block *pred : pred_it->second) {
        if (pred == backedge_block) {
          continue;
        }
        if (preheader != nullptr && pred != preheader) {
          preheader = nullptr;
          break;
        }
        preheader = pred;
      }
      if (preheader == nullptr) {
        continue;
      }

      std::unordered_set<Block *> loop_blocks;
      loop_blocks.insert(cond_block);
      loop_blocks.insert(body_block);
      if (inc_block != nullptr) {
        loop_blocks.insert(inc_block);
      }

      int begin_count = 0;
      int end_count = 0;
      Block *begin_block = nullptr;
      Block *end_block = nullptr;
      size_t begin_index = 0;
      size_t end_index = 0;

      for (Block *loop_block : loop_blocks) {
        const auto &insts = loop_block->getInstructions();
        for (size_t idx = 0; idx < insts.size(); ++idx) {
          const Instruction *inst = insts[idx].get();
          if (inst == nullptr) {
            continue;
          }
          if (inst->getKind() == InstKind::GpuScopeBegin) {
            begin_count++;
            begin_block = loop_block;
            begin_index = idx;
          } else if (inst->getKind() == InstKind::GpuScopeEnd) {
            end_count++;
            end_block = loop_block;
            end_index = idx;
          }
        }
      }

      if (begin_count != 1 || end_count != 1 || begin_block != body_block ||
          end_block != body_block || begin_index >= end_index) {
        continue;
      }

      auto &body_insts = body_block->getInstructionsMut();
      std::unique_ptr<Instruction> lifted_begin =
          std::move(body_insts[begin_index]);
      body_insts.erase(body_insts.begin() + static_cast<std::ptrdiff_t>(begin_index));
      if (end_index > begin_index) {
        end_index--;
      }
      std::unique_ptr<Instruction> lifted_end =
          std::move(body_insts[end_index]);
      body_insts.erase(body_insts.begin() + static_cast<std::ptrdiff_t>(end_index));

      auto &preheader_insts = preheader->getInstructionsMut();
      size_t preheader_insert = preheader_insts.size();
      if (preheader_insert > 0) {
        Instruction *last = preheader_insts.back().get();
        if (last != nullptr && isTerminatorKind(last->getKind())) {
          preheader_insert--;
        }
      }
      preheader_insts.insert(
          preheader_insts.begin() + static_cast<std::ptrdiff_t>(preheader_insert),
          std::move(lifted_begin));

      auto &exit_insts = exit_block->getInstructionsMut();
      exit_insts.insert(exit_insts.begin(), std::move(lifted_end));

      changed = true;
    }
  }

  return changed;
}

} // namespace neuron::nir

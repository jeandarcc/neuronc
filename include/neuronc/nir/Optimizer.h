#pragma once

#include "neuronc/nir/NIR.h"
#include <memory>

namespace neuron {
namespace nir {

class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;
    virtual bool runOnModule(Module* module) = 0;
    virtual const char* getName() const = 0;
};

class ConstantFoldingPass : public OptimizationPass {
public:
    bool runOnModule(Module* module) override;
    const char* getName() const override { return "Constant Folding"; }
private:
    bool foldBlock(Block* block, Module* module);
    Value* makeIntConstant(int64_t value);
    Value* makeFloatConstant(double value);
    std::vector<std::unique_ptr<Value>> m_ownedConstants;
};

class CopyPropagationPass : public OptimizationPass {
public:
    bool runOnModule(Module* module) override;
    const char* getName() const override { return "Copy Propagation"; }
};

class AlgebraicSimplificationPass : public OptimizationPass {
public:
    bool runOnModule(Module* module) override;
    const char* getName() const override { return "Algebraic Simplification"; }
private:
    Value* makeIntConstant(int64_t value);
    Value* makeFloatConstant(double value);
    std::vector<std::unique_ptr<Value>> m_ownedConstants;
};

class DeadStoreEliminationPass : public OptimizationPass {
public:
    bool runOnModule(Module* module) override;
    const char* getName() const override { return "Dead Store Elimination"; }
};

class BranchSimplificationPass : public OptimizationPass {
public:
    bool runOnModule(Module* module) override;
    const char* getName() const override { return "Branch Simplification"; }
};

class DeadCodeEliminationPass : public OptimizationPass {
public:
    bool runOnModule(Module* module) override;
    const char* getName() const override { return "Dead Code Elimination"; }
private:
    bool elideDeadCode(Block* block);
};

class TensorFusionPass : public OptimizationPass {
public:
    bool runOnModule(Module* module) override;
    const char* getName() const override { return "Tensor Fusion"; }
};

class GpuScopeLiftingPass : public OptimizationPass {
public:
    bool runOnModule(Module* module) override;
    const char* getName() const override { return "GPU Scope Lifting"; }
};

class Optimizer {
public:
    void addPass(std::unique_ptr<OptimizationPass> pass) {
        m_passes.push_back(std::move(pass));
    }

    void run(Module* module) {
        bool changed;
        do {
            changed = false;
            for (auto& pass : m_passes) {
                if (pass->runOnModule(module)) {
                    changed = true;
                }
            }
        } while (changed); // run until fixed point
    }

    static std::unique_ptr<Optimizer> createDefaultOptimizer() {
        auto opt = std::make_unique<Optimizer>();
        opt->addPass(std::make_unique<ConstantFoldingPass>());
        opt->addPass(std::make_unique<CopyPropagationPass>());
        opt->addPass(std::make_unique<AlgebraicSimplificationPass>());
        opt->addPass(std::make_unique<DeadCodeEliminationPass>());
        opt->addPass(std::make_unique<BranchSimplificationPass>());
        opt->addPass(std::make_unique<TensorFusionPass>());
        opt->addPass(std::make_unique<GpuScopeLiftingPass>());
        opt->addPass(std::make_unique<DeadCodeEliminationPass>());
        return opt;
    }

private:
    std::vector<std::unique_ptr<OptimizationPass>> m_passes;
};

} // namespace nir
} // namespace neuron

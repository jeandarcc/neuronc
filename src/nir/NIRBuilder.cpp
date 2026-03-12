#include "neuronc/nir/NIRBuilder.h"

#include "detail/NIRBuilderShared.h"

namespace neuron::nir {

void NIRBuilder::applyExecutionHint(Instruction *inst) {
  if (inst == nullptr || !detail::isTensorInstKind(inst->getKind())) {
    return;
  }
  const ExecutionHint hint =
      m_gpuDepth > 0 ? ExecutionHint::GpuPrefer : ExecutionHint::Auto;
  inst->setExecutionHint(hint);
}

void NIRBuilder::emitGpuScopeBeginInst(int32_t preferenceMode,
                                       int32_t preferenceTarget) {
  auto *beginInst = createInst(InstKind::GpuScopeBegin, NType::makeVoid(), "");
  beginInst->addOperand(new ConstantInt(preferenceMode));
  beginInst->addOperand(new ConstantInt(preferenceTarget));
}

void NIRBuilder::emitGpuScopeEndInst() {
  (void)createInst(InstKind::GpuScopeEnd, NType::makeVoid(), "");
}

void NIRBuilder::unwindGpuScopesForReturn() {
  while (!m_gpuRuntimeScopeStack.empty()) {
    emitGpuScopeEndInst();
    m_gpuRuntimeScopeStack.pop_back();
  }
}

void NIRBuilder::unwindGpuScopesForBreak() {
  const std::size_t currentBreakDepth = m_breakTargetStack.size();
  while (!m_gpuRuntimeScopeStack.empty() &&
         m_gpuRuntimeScopeStack.back().breakDepthAtEntry >=
             currentBreakDepth) {
    emitGpuScopeEndInst();
    m_gpuRuntimeScopeStack.pop_back();
  }
}

void NIRBuilder::unwindGpuScopesForContinue() {
  const std::size_t currentContinueDepth = m_continueTargetStack.size();
  while (!m_gpuRuntimeScopeStack.empty() &&
         m_gpuRuntimeScopeStack.back().continueDepthAtEntry >=
             currentContinueDepth) {
    emitGpuScopeEndInst();
    m_gpuRuntimeScopeStack.pop_back();
  }
}

} // namespace neuron::nir

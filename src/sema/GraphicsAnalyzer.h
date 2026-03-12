#pragma once

#include <string>

namespace neuron {
class CanvasStmtNode;
class GpuBlockNode;
class MethodDeclNode;
class ShaderDeclNode;
} // namespace neuron

namespace neuron::sema_detail {

class SemanticDriver;

class GraphicsAnalyzer {
public:
  explicit GraphicsAnalyzer(SemanticDriver &driver);

  void visitCanvasStmt(CanvasStmtNode *node);
  void visitShaderDecl(ShaderDeclNode *node);
  void visitGpuBlock(GpuBlockNode *node);

private:
  void registerShaderDescriptorMethod(MethodDeclNode *method,
                                      const std::string &shaderName);
  void validateCpuSideDescriptorMethod(MethodDeclNode *method,
                                       const std::string &shaderName);
  SemanticDriver &m_driver;
};

} // namespace neuron::sema_detail

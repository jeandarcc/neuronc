#pragma once

namespace neuron {
class BindingDeclNode;
} // namespace neuron

namespace neuron::sema_detail {

class SemanticDriver;

class BindingAnalyzer {
public:
  explicit BindingAnalyzer(SemanticDriver &driver);

  void visit(BindingDeclNode *node);

private:
  SemanticDriver &m_driver;
};

} // namespace neuron::sema_detail

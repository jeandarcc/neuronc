#pragma once

namespace neuron {
class CallExprNode;
} // namespace neuron

namespace neuron::sema_detail {

class SemanticDriver;

class CallableBinder {
public:
  explicit CallableBinder(SemanticDriver &driver);

  bool bindNamedCallArguments(CallExprNode *node);

private:
  SemanticDriver &m_driver;
};

} // namespace neuron::sema_detail

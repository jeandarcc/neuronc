#pragma once

#include <string>
#include <string_view>

namespace neuron {
struct SourceLocation;
class BlockNode;
class MethodDeclNode;
} // namespace neuron

namespace neuron::sema_detail {

class SemanticDriver;

class RuleValidator {
public:
  explicit RuleValidator(SemanticDriver &driver);

  void validateMethodName(const std::string &methodName,
                          const SourceLocation &loc);
  void validateVariableName(const std::string &variableName,
                            const SourceLocation &loc, bool isConst);
  void validateBlockLength(const BlockNode *block, const SourceLocation &loc,
                           std::string_view ownerName,
                           std::string_view ownerKind);
  bool hasRequiredPublicMethodDocs(const MethodDeclNode *node) const;

private:
  void validateConstVariableName(const std::string &variableName,
                                 const SourceLocation &loc);

  SemanticDriver &m_driver;
};

} // namespace neuron::sema_detail

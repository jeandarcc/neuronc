#include "RuleValidator.h"

#include "AnalysisHelpers.h"
#include "SemanticDriver.h"

#include <cctype>

namespace neuron::sema_detail {

RuleValidator::RuleValidator(SemanticDriver &driver) : m_driver(driver) {}

void RuleValidator::validateMethodName(const std::string &methodName,
                                       const SourceLocation &loc) {
  if (methodName.empty() || methodName == "constructor") {
    return;
  }

  const AnalysisOptions &options = m_driver.context().options();
  if (options.minMethodNameLength > 0 &&
      static_cast<int>(methodName.size()) < options.minMethodNameLength) {
    m_driver.context().errorWithAgentHint(
        loc,
        "Invalid method name '" + methodName +
            "': method name must be at least " +
            std::to_string(options.minMethodNameLength) + " characters.",
        "Use descriptive method names to satisfy min_method_name_length.");
    return;
  }

  const unsigned char first = static_cast<unsigned char>(methodName.front());
  if (std::isdigit(first)) {
    m_driver.context().error(loc, "Invalid method name '" + methodName +
                                      "': method name cannot start with a digit.");
    return;
  }
  if (!std::isalpha(first)) {
    m_driver.context().error(loc, "Invalid method name '" + methodName +
                                      "': method name must start with a letter.");
    return;
  }

  for (char ch : methodName) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (!std::isalnum(c)) {
      m_driver.context().error(loc, "Invalid method name '" + methodName +
                                        "': only letters and digits are allowed.");
      return;
    }
  }

  if (options.requireMethodUppercaseStart && !std::isupper(first)) {
    m_driver.context().errorWithAgentHint(
        loc,
        "Invalid method name '" + methodName +
            "': method name must start with an uppercase letter.",
        "Rename method to PascalCase (for example: `ComputeTotal`).");
  }
}

void RuleValidator::validateVariableName(const std::string &variableName,
                                         const SourceLocation &loc,
                                         bool isConst) {
  if (variableName.empty()) {
    return;
  }

  if (isConst && m_driver.context().options().requireConstUppercase) {
    validateConstVariableName(variableName, loc);
    return;
  }

  const unsigned char first = static_cast<unsigned char>(variableName.front());
  const bool startsWithUnderscore = variableName.front() == '_';
  if (startsWithUnderscore) {
    if (variableName.size() == 1) {
      m_driver.context().error(
          loc, "Invalid variable name '_': expected pattern like _testObject.");
      return;
    }
    const unsigned char second = static_cast<unsigned char>(variableName[1]);
    if (!std::islower(second)) {
      m_driver.context().error(
          loc, "Invalid variable name '" + variableName +
                   "': when using '_', the next character must be lowercase.");
      return;
    }
  } else if (!std::islower(first)) {
    m_driver.context().error(
        loc, "Invalid variable name '" + variableName +
                 "': variable name must start with a lowercase letter or '_'.");
    return;
  }

  for (std::size_t i = 0; i < variableName.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(variableName[i]);
    if (variableName[i] == '_') {
      if (i != 0) {
        m_driver.context().error(
            loc, "Invalid variable name '" + variableName +
                     "': '_' is only allowed as the first character.");
        return;
      }
      continue;
    }
    if (!std::isalnum(c)) {
      m_driver.context().error(
          loc, "Invalid variable name '" + variableName +
                   "': only letters, digits, and an optional leading '_' are "
                   "allowed.");
      return;
    }
  }
}

void RuleValidator::validateBlockLength(const BlockNode *block,
                                        const SourceLocation &loc,
                                        std::string_view ownerName,
                                        std::string_view ownerKind) {
  const AnalysisOptions &options = m_driver.context().options();
  if (options.maxLinesPerBlockStatement <= 0 || block == nullptr ||
      !block->hasExplicitBraces || block->endLine < block->location.line) {
    return;
  }

  const int blockLines = block->endLine - block->location.line + 1;
  if (blockLines <= options.maxLinesPerBlockStatement) {
    return;
  }

  std::string subject = "Block statement";
  if (!ownerName.empty() && !ownerKind.empty()) {
    subject = std::string(ownerKind) + " '" + std::string(ownerName) + "'";
  } else if (!ownerKind.empty()) {
    subject = std::string(ownerKind);
  }

  m_driver.context().errorWithAgentHint(
      loc,
      subject + " exceeds maximum allowed length (" +
          std::to_string(blockLines) + " lines, limit " +
          std::to_string(options.maxLinesPerBlockStatement) + ").",
      "Split large blocks into smaller helpers or early exits.");
}

bool RuleValidator::hasRequiredPublicMethodDocs(
    const MethodDeclNode *node) const {
  return m_driver.context().diagnostics().hasRequiredPublicMethodDocs(node);
}

void RuleValidator::validateConstVariableName(const std::string &variableName,
                                              const SourceLocation &loc) {
  if (isUpperSnakeCase(variableName)) {
    return;
  }

  m_driver.context().errorWithAgentHint(
      loc,
      "Invalid const variable name '" + variableName +
          "': const names must use uppercase letters, digits, and '_' only.",
      "Rename const variables to UPPER_SNAKE_CASE (for example: `MAX_BUFFER_SIZE`).");
}

} // namespace neuron::sema_detail

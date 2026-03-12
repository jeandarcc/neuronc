#include "CallableBinder.h"

#include "AnalysisContext.h"
#include "AnalysisHelpers.h"
#include "SemanticDriver.h"

namespace neuron::sema_detail {

CallableBinder::CallableBinder(SemanticDriver &driver) : m_driver(driver) {}

bool CallableBinder::bindNamedCallArguments(CallExprNode *node) {
  if (node == nullptr || node->arguments.empty()) {
    return true;
  }
  if (node->argumentLabels.size() != node->arguments.size()) {
    node->argumentLabels.assign(node->arguments.size(), "");
    return true;
  }

  bool hasNamedArgs = false;
  for (const auto &label : node->argumentLabels) {
    if (!label.empty()) {
      hasNamedArgs = true;
      break;
    }
  }
  if (!hasNamedArgs) {
    return true;
  }

  const std::string callableName = resolveCallName(node->callee.get());
  if (callableName.empty()) {
    m_driver.context().error(
        node->location,
        "Named arguments are only supported on identifier-style call targets");
    return false;
  }

  const auto *paramNames = m_driver.context().findNamedCallableSignature(
      callableName, node->arguments.size(), node->location);
  if (paramNames == nullptr) {
    return false;
  }

  std::unordered_map<std::string, std::size_t> parameterIndexByName;
  parameterIndexByName.reserve(paramNames->size());
  for (std::size_t i = 0; i < paramNames->size(); ++i) {
    parameterIndexByName.emplace((*paramNames)[i], i);
  }

  std::vector<int> paramToArgIndex(paramNames->size(), -1);
  std::size_t nextPositionalParam = 0;
  bool namedSectionStarted = false;
  bool success = true;

  for (std::size_t argIndex = 0; argIndex < node->arguments.size(); ++argIndex) {
    const std::string &label = node->argumentLabels[argIndex];
    if (label.empty()) {
      if (namedSectionStarted) {
        m_driver.context().error(
            node->location,
            "Positional arguments cannot appear after named arguments in call "
            "to '" +
                callableName + "'");
        success = false;
        continue;
      }

      while (nextPositionalParam < paramToArgIndex.size() &&
             paramToArgIndex[nextPositionalParam] != -1) {
        ++nextPositionalParam;
      }
      if (nextPositionalParam >= paramToArgIndex.size()) {
        m_driver.context().error(node->location,
                                 "Too many arguments in call to '" +
                                     callableName + "'");
        success = false;
        continue;
      }

      paramToArgIndex[nextPositionalParam] = static_cast<int>(argIndex);
      ++nextPositionalParam;
      continue;
    }

    namedSectionStarted = true;
    auto paramIt = parameterIndexByName.find(label);
    if (paramIt == parameterIndexByName.end()) {
      m_driver.context().error(node->location, "Unknown named argument '" + label +
                                                  "' in call to '" +
                                                  callableName + "'");
      success = false;
      continue;
    }

    const std::size_t paramIndex = paramIt->second;
    if (paramToArgIndex[paramIndex] != -1) {
      m_driver.context().error(node->location,
                               "Argument '" + label +
                                   "' is assigned more than once in call to '" +
                                   callableName + "'");
      success = false;
      continue;
    }

    paramToArgIndex[paramIndex] = static_cast<int>(argIndex);
  }

  for (std::size_t paramIndex = 0; paramIndex < paramToArgIndex.size();
       ++paramIndex) {
    if (paramToArgIndex[paramIndex] == -1) {
      m_driver.context().error(node->location,
                               "Missing argument for parameter '" +
                                   (*paramNames)[paramIndex] + "' in call to '" +
                                   callableName + "'");
      success = false;
    }
  }

  if (!success) {
    return false;
  }

  std::vector<ASTNodePtr> reorderedArgs;
  reorderedArgs.reserve(paramToArgIndex.size());
  for (std::size_t paramIndex = 0; paramIndex < paramToArgIndex.size();
       ++paramIndex) {
    reorderedArgs.push_back(std::move(node->arguments[static_cast<std::size_t>(
        paramToArgIndex[paramIndex])]));
  }
  node->arguments = std::move(reorderedArgs);
  node->argumentLabels.assign(node->arguments.size(), "");
  return true;
}

} // namespace neuron::sema_detail

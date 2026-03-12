#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace neuron::diagnostics {

struct DiagnosticArgument {
  std::string name;
  std::string value;
};

using DiagnosticArguments = std::vector<DiagnosticArgument>;

inline std::optional<std::string_view>
findDiagnosticArgument(const DiagnosticArguments &arguments,
                       std::string_view name) {
  for (const auto &argument : arguments) {
    if (argument.name == name) {
      return argument.value;
    }
  }
  return std::nullopt;
}

} // namespace neuron::diagnostics

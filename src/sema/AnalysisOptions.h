#pragma once

#include <string>

namespace neuron::sema_detail {

struct AnalysisOptions {
  int maxClassesPerFile = 0;
  bool requireMethodUppercaseStart = false;
  bool enforceStrictFileNamingRules = false;
  std::string sourceFileStem;
  int maxLinesPerMethod = 0;
  int maxLinesPerBlockStatement = 0;
  int minMethodNameLength = 0;
  bool requireClassExplicitVisibility = false;
  bool requirePropertyExplicitVisibility = false;
  bool requireConstUppercase = false;
  int maxNestingDepth = 0;
  bool requirePublicMethodDocs = false;
};

} // namespace neuron::sema_detail

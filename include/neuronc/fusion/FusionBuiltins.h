#pragma once

#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace neuron {

enum class FusionBuiltinKind {
  None = 0,
  Conv2DBatchNormRelu,
};

struct FusionPatternSpec {
  FusionBuiltinKind kind = FusionBuiltinKind::None;
  const char *builtinName = nullptr;
  std::size_t argumentCount = 0;
  std::array<const char *, 4> stages{};
  std::size_t stageCount = 0;
};

inline constexpr int kFusionBuiltinPreferGpuExecHint = 1;

inline constexpr FusionPatternSpec kFusionPatternSpecs[] = {{
    FusionBuiltinKind::Conv2DBatchNormRelu,
    "__neuron_fused_conv2d_batchnorm_relu",
    12u,
    {"conv2d", "batchnorm", "relu", nullptr},
    3u,
}};

inline std::string canonicalizeFusionStageName(std::string name) {
  const std::size_t dot = name.rfind('.');
  if (dot != std::string::npos) {
    name = name.substr(dot + 1);
  }

  std::string canonical;
  canonical.reserve(name.size());
  for (char ch : name) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (std::isalnum(uch) != 0) {
      canonical.push_back(
          static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
  }
  return canonical;
}

inline const FusionPatternSpec *
findFusionPatternSpec(FusionBuiltinKind kind) {
  for (const FusionPatternSpec &spec : kFusionPatternSpecs) {
    if (spec.kind == kind) {
      return &spec;
    }
  }
  return nullptr;
}

inline const FusionPatternSpec *
matchFusionPatternSpec(const std::vector<std::string> &callNames) {
  for (const FusionPatternSpec &spec : kFusionPatternSpecs) {
    if (callNames.size() != spec.stageCount) {
      continue;
    }

    bool matched = true;
    for (std::size_t i = 0; i < spec.stageCount; ++i) {
      if (canonicalizeFusionStageName(callNames[i]) != spec.stages[i]) {
        matched = false;
        break;
      }
    }
    if (matched) {
      return &spec;
    }
  }
  return nullptr;
}

inline const char *fusionBuiltinName(FusionBuiltinKind kind) {
  const FusionPatternSpec *spec = findFusionPatternSpec(kind);
  return spec != nullptr ? spec->builtinName : nullptr;
}

} // namespace neuron

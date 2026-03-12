#pragma once

#include <string>
#include <vector>

namespace neuron {

enum class NconNetworkPolicy { Deny, Allow };

struct NconPermissionConfig {
  std::vector<std::string> fsRead = {"res:/"};
  std::vector<std::string> fsWrite = {"work:/"};
  NconNetworkPolicy network = NconNetworkPolicy::Deny;
  bool processSpawnAllowed = false;
};

} // namespace neuron

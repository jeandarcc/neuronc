#pragma once

#include <filesystem>
#include <string>

namespace neuron::lsp {

class NeuronLspServer {
public:
  explicit NeuronLspServer(std::filesystem::path toolRoot = {});
  int run();

private:
  std::filesystem::path m_toolRoot;
  std::string m_userLanguagePreference;
};

} // namespace neuron::lsp

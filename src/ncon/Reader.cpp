#include "neuronc/ncon/Reader.h"

namespace neuron::ncon {

bool loadContainer(const std::filesystem::path &inputPath,
                   ContainerData *outContainer, std::string *outError) {
  return readContainer(inputPath, outContainer, outError);
}

} // namespace neuron::ncon

#include "neuronc/ncon/NconCLI.h"

int main(int argc, char *argv[]) {
  return neuron::ncon::runCli(argc, argv, argc > 0 ? argv[0] : nullptr);
}

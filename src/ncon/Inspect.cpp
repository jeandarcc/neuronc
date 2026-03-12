#include "neuronc/ncon/Inspect.h"

#include <sstream>

namespace neuron::ncon {

std::string inspectContainerHuman(const ContainerData &container) {
  std::ostringstream out;
  out << "NCON Container\n";
  out << "Module: " << container.program.moduleName << "\n";
  out << "Entry function id: " << container.program.entryFunctionId << "\n";
  out << "Strings: " << container.program.strings.size() << "\n";
  out << "Types: " << container.program.types.size() << "\n";
  out << "Constants: " << container.program.constants.size() << "\n";
  out << "Globals: " << container.program.globals.size() << "\n";
  out << "Functions: " << container.program.functions.size() << "\n";
  out << "Blocks: " << container.program.blocks.size() << "\n";
  out << "Instructions: " << container.program.instructions.size() << "\n";
  out << "Operands: " << container.program.operands.size() << "\n";
  out << "Resources: " << container.resources.size() << "\n";
  out << "\nManifest:\n" << container.manifestJson;
  if (!container.debugMap.empty()) {
    out << "\nDebug Map:\n" << container.debugMap;
  }
  return out.str();
}

std::string inspectContainerJson(const ContainerData &container) {
  std::ostringstream out;
  out << "{\n";
  out << "  \"manifest\": " << container.manifestJson << ",\n";
  out << "  \"bytecode\": {\n";
  out << "    \"module\": \"" << container.program.moduleName << "\",\n";
  out << "    \"entry_function_id\": " << container.program.entryFunctionId
      << ",\n";
  out << "    \"strings\": " << container.program.strings.size() << ",\n";
  out << "    \"types\": " << container.program.types.size() << ",\n";
  out << "    \"constants\": " << container.program.constants.size() << ",\n";
  out << "    \"globals\": " << container.program.globals.size() << ",\n";
  out << "    \"functions\": " << container.program.functions.size() << ",\n";
  out << "    \"blocks\": " << container.program.blocks.size() << ",\n";
  out << "    \"instructions\": " << container.program.instructions.size()
      << ",\n";
  out << "    \"operands\": " << container.program.operands.size() << "\n";
  out << "  },\n";
  out << "  \"resources\": [\n";
  for (size_t i = 0; i < container.resources.size(); ++i) {
    const auto &resource = container.resources[i];
    out << "    {\"id\": \"" << resource.id << "\", \"size\": "
        << resource.size << ", \"crc32\": " << resource.crc32 << "}";
    out << (i + 1 == container.resources.size() ? "\n" : ",\n");
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

} // namespace neuron::ncon

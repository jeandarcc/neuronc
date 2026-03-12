#include "neuronc/ncon/VM.h"

#include <filesystem>

using namespace neuron;

static ncon::ContainerData makeVmTestContainer() {
  ncon::ContainerData container;
  container.manifestJson =
      "{\n"
      "  \"app\": {\n"
      "    \"name\": \"vm_demo\",\n"
      "    \"version\": \"0.1.0\",\n"
      "    \"entry_module\": \"vm_demo\",\n"
      "    \"entry_function\": \"Init\",\n"
      "    \"instruction_set_version\": 1\n"
      "  },\n"
      "  \"runtime\": {\n"
      "    \"abi\": \"ncon-runtime-v1\",\n"
      "    \"tensor_profile\": \"balanced\",\n"
      "    \"tensor_autotune\": true,\n"
      "    \"hot_reload\": false,\n"
      "    \"native\": {\n"
      "      \"enabled\": false,\n"
      "      \"modules\": []\n"
      "    }\n"
      "  },\n"
      "  \"permissions\": {\n"
      "    \"fs_read\": [],\n"
      "    \"fs_write\": [],\n"
      "    \"network\": \"deny\",\n"
      "    \"process_spawn\": \"deny\"\n"
      "  },\n"
      "  \"resources\": [],\n"
      "  \"build\": {\n"
      "    \"source_hash\": \"test\",\n"
      "    \"optimize\": \"aggressive\",\n"
      "    \"target_cpu\": \"generic\"\n"
      "  }\n"
      "}\n";
  container.program.moduleName = "vm_demo";
  container.program.entryFunctionId = 0;
  container.program.strings = {"Init"};
  container.program.types.push_back(
      {TypeKind::Int, ncon::kInvalidIndex, ncon::kInvalidIndex, ncon::kInvalidIndex});
  container.program.types.push_back(
      {TypeKind::Void, ncon::kInvalidIndex, ncon::kInvalidIndex, ncon::kInvalidIndex});
  container.program.constants.push_back(
      {ncon::ConstantKind::Int, 0, 40, 0.0, ncon::kInvalidIndex});
  container.program.constants.push_back(
      {ncon::ConstantKind::Int, 0, 2, 0.0, ncon::kInvalidIndex});
  container.program.functions.push_back({0, 0, 0, 1, 0, 1, 0, {}});
  container.program.blocks.push_back({ncon::kInvalidIndex, 0, 2});
  container.program.instructions.push_back(
      {static_cast<uint16_t>(ncon::Opcode::Add), 0, 0, 0, 0, 2, 0, 0});
  container.program.instructions.push_back(
      {static_cast<uint16_t>(ncon::Opcode::Ret), 0, ncon::kInvalidIndex, 0, 2, 1, 0, 0});
  container.program.operands.push_back({ncon::OperandKind::Constant, 0});
  container.program.operands.push_back({ncon::OperandKind::Constant, 1});
  container.program.operands.push_back({ncon::OperandKind::Slot, 0});
  return container;
}

static ncon::ContainerData makeVmCastContainer(
    uint32_t targetTypeId, ncon::ConstantRecord constant) {
  ncon::ContainerData container;
  container.manifestJson =
      "{\n"
      "  \"app\": {\n"
      "    \"name\": \"vm_cast\",\n"
      "    \"version\": \"0.1.0\",\n"
      "    \"entry_module\": \"vm_cast\",\n"
      "    \"entry_function\": \"Init\",\n"
      "    \"instruction_set_version\": 1\n"
      "  },\n"
      "  \"runtime\": {\n"
      "    \"abi\": \"ncon-runtime-v1\",\n"
      "    \"tensor_profile\": \"balanced\",\n"
      "    \"tensor_autotune\": true,\n"
      "    \"hot_reload\": false,\n"
      "    \"native\": {\n"
      "      \"enabled\": false,\n"
      "      \"modules\": []\n"
      "    }\n"
      "  },\n"
      "  \"permissions\": {\n"
      "    \"fs_read\": [],\n"
      "    \"fs_write\": [],\n"
      "    \"network\": \"deny\",\n"
      "    \"process_spawn\": \"deny\"\n"
      "  },\n"
      "  \"resources\": [],\n"
      "  \"build\": {\n"
      "    \"source_hash\": \"test\",\n"
      "    \"optimize\": \"aggressive\",\n"
      "    \"target_cpu\": \"generic\"\n"
      "  }\n"
      "}\n";
  container.program.moduleName = "vm_cast";
  container.program.entryFunctionId = 0;
  container.program.strings = {"Init", "int", "float", "maybe", "bad"};

  ncon::TypeRecord intType;
  intType.kind = TypeKind::Int;
  intType.nameStringId = 1;
  container.program.types.push_back(intType); // 0

  ncon::TypeRecord floatType;
  floatType.kind = TypeKind::Float;
  floatType.nameStringId = 2;
  container.program.types.push_back(floatType); // 1

  ncon::TypeRecord maybeFloatType;
  maybeFloatType.kind = TypeKind::Nullable;
  maybeFloatType.nameStringId = 3;
  maybeFloatType.genericTypeIds.push_back(1);
  container.program.types.push_back(maybeFloatType); // 2

  ncon::TypeRecord maybeIntType;
  maybeIntType.kind = TypeKind::Nullable;
  maybeIntType.nameStringId = 3;
  maybeIntType.genericTypeIds.push_back(0);
  container.program.types.push_back(maybeIntType); // 3

  ncon::TypeRecord stringType;
  stringType.kind = TypeKind::String;
  container.program.types.push_back(stringType); // 4

  ncon::TypeRecord voidType;
  voidType.kind = TypeKind::Void;
  container.program.types.push_back(voidType); // 5

  container.program.constants.push_back(constant);
  container.program.functions.push_back(
      {0, targetTypeId, 0, 1, 0, 1, 0, {}});
  container.program.blocks.push_back({ncon::kInvalidIndex, 0, 2});
  container.program.instructions.push_back(
      {static_cast<uint16_t>(ncon::Opcode::Cast), 0, 0, targetTypeId, 0, 1, 0, 0});
  container.program.instructions.push_back(
      {static_cast<uint16_t>(ncon::Opcode::Ret), 0, ncon::kInvalidIndex,
       targetTypeId, 1, 1, 0, 0});
  container.program.operands.push_back({ncon::OperandKind::Constant, 0});
  container.program.operands.push_back({ncon::OperandKind::Slot, 0});
  return container;
}

TEST(NconVmRunsSimpleProgram) {
  ncon::ContainerData container = makeVmTestContainer();
  ncon::SandboxContext sandbox;
  sandbox.workDirectory = std::filesystem::temp_directory_path() / "ncon-vm-test";

  ncon::VM vm(container, sandbox);
  std::string error;
  ASSERT_TRUE(vm.run(&error));
  ASSERT_TRUE(error.empty());
  return true;
}

TEST(NconVmRunsSuccessfulNullableCastProgram) {
  ncon::ConstantRecord constant;
  constant.kind = ncon::ConstantKind::Int;
  constant.typeId = 0;
  constant.intValue = 10;

  ncon::ContainerData container = makeVmCastContainer(2, constant);
  ncon::SandboxContext sandbox;
  sandbox.workDirectory =
      std::filesystem::temp_directory_path() / "ncon-vm-cast-success";

  ncon::VM vm(container, sandbox);
  std::string error;
  ASSERT_TRUE(vm.run(&error));
  ASSERT_TRUE(error.empty());
  return true;
}

TEST(NconVmRejectsInvalidRequiredCastProgram) {
  ncon::ConstantRecord constant;
  constant.kind = ncon::ConstantKind::String;
  constant.typeId = 4;
  constant.stringId = 4;

  ncon::ContainerData container = makeVmCastContainer(0, constant);
  ncon::SandboxContext sandbox;
  sandbox.workDirectory =
      std::filesystem::temp_directory_path() / "ncon-vm-cast-fail";

  ncon::VM vm(container, sandbox);
  std::string error;
  ASSERT_FALSE(vm.run(&error));
  ASSERT_TRUE(error.find("cast failed") != std::string::npos);
  return true;
}

TEST(NconVmAllowsInvalidNullableCastProgram) {
  ncon::ConstantRecord constant;
  constant.kind = ncon::ConstantKind::String;
  constant.typeId = 4;
  constant.stringId = 4;

  ncon::ContainerData container = makeVmCastContainer(3, constant);
  ncon::SandboxContext sandbox;
  sandbox.workDirectory =
      std::filesystem::temp_directory_path() / "ncon-vm-cast-nullable";

  ncon::VM vm(container, sandbox);
  std::string error;
  ASSERT_TRUE(vm.run(&error));
  ASSERT_TRUE(error.empty());
  return true;
}

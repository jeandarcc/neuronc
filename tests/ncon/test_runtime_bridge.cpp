#include "neuronc/ncon/RuntimeBridge.h"

#include <filesystem>
#include <fstream>

using namespace neuron;
namespace fs = std::filesystem;

TEST(NconRuntimeBridgeResourceBuiltins) {
  NconPermissionConfig permissions;
  permissions.fsRead = {"res:/"};

  ncon::SandboxContext sandbox;
  std::string error;
  ASSERT_TRUE(ncon::initializeSandbox("resource-builtin-test", permissions,
                                      &sandbox, &error));

  const fs::path resourcePath = sandbox.resourceDirectory / "config" / "tokenizer.txt";
  fs::create_directories(resourcePath.parent_path());
  std::ofstream out(resourcePath, std::ios::binary | std::ios::trunc);
  out << "hello";
  out.close();
  sandbox.mountedResources["config/tokenizer.txt"] = resourcePath;

  ncon::RuntimeBridge bridge(&sandbox);
  ASSERT_TRUE(bridge.isBuiltin("Resource.Exists"));
  ASSERT_TRUE(bridge.isBuiltin("Resource.ReadText"));
  ASSERT_TRUE(bridge.isBuiltin("Resource.ReadBytes"));

  ncon::Program program;
  ncon::VMValue existsResult;
  std::vector<ncon::VMValue> existsArgs(1);
  existsArgs[0].data = std::string("config/tokenizer.txt");
  ASSERT_TRUE(bridge.invokeBuiltin(program, "Resource.Exists", existsArgs,
                                   &existsResult, &error));
  ASSERT_EQ(std::get<int64_t>(existsResult.data), 1);

  ncon::VMValue textResult;
  ASSERT_TRUE(bridge.invokeBuiltin(program, "Resource.ReadText", existsArgs,
                                   &textResult, &error));
  ASSERT_EQ(std::get<std::string>(textResult.data), "hello");

  ncon::VMValue bytesResult;
  ASSERT_TRUE(bridge.invokeBuiltin(program, "Resource.ReadBytes", existsArgs,
                                   &bytesResult, &error));
  auto bytes = std::get<ncon::ArrayIntHandle>(bytesResult.data);
  ASSERT_TRUE(bytes != nullptr);
  ASSERT_EQ(bytes->size(), 5u);
  ASSERT_EQ((*bytes)[0], static_cast<int64_t>('h'));
  ASSERT_EQ((*bytes)[4], static_cast<int64_t>('o'));
  return true;
}

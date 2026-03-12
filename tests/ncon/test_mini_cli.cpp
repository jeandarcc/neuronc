#include "neuronc/ncon/NconMiniCLI.h"

using namespace neuron;

TEST(NconMiniCliParsesDirectContainerRun) {
  const ncon::MiniCliAction action =
      ncon::parseMiniCliAction({"demo.ncon"});
  ASSERT_EQ(action.kind, ncon::MiniCliActionKind::RunContainer);
  ASSERT_EQ(action.containerPath.string(), "demo.ncon");
  return true;
}

TEST(NconMiniCliRejectsRunSubcommandSyntax) {
  const ncon::MiniCliAction action =
      ncon::parseMiniCliAction({"run", "demo.ncon"});
  ASSERT_EQ(action.kind, ncon::MiniCliActionKind::Error);
  ASSERT_TRUE(action.error.find("nucleus <file.ncon>") != std::string::npos);
  return true;
}

TEST(NconMiniCliParsesSandboxHelperCommand) {
  const ncon::MiniCliAction action =
      ncon::parseMiniCliAction({"__sandbox_run", "staged.ncon"});
  ASSERT_EQ(action.kind, ncon::MiniCliActionKind::RunSandboxHelper);
  ASSERT_EQ(action.containerPath.string(), "staged.ncon");
  return true;
}

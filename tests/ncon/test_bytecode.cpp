#include "neuronc/ncon/Bytecode.h"
#include "neuronc/lexer/Lexer.h"
#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/parser/Parser.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include <memory>
#include <string>

using namespace neuron;

static std::unique_ptr<nir::Module>
buildNconCoverageModule(std::string *outError) {
  const std::string source =
      "Init method() as int {\n"
      "  color is Color.Red as Color;\n"
      "  value is 10 as int;\n"
      "  value as maybe dynamic then string then float;\n"
      "  match (color) {\n"
      "    Color.Red then {\n"
      "      Print(\"Red\");\n"
      "    }\n"
      "    Color.Yellow then {\n"
      "      Print(\"Yellow\");\n"
      "    }\n"
      "    Color.Green then {\n"
      "      Print(\"Green\");\n"
      "    }\n"
      "    default then {\n"
      "      Print(\"Unknown\");\n"
      "    }\n"
      "  }\n"
      "  return 0;\n"
      "}\n"
      "Color enum {\n"
      "  Red,\n"
      "  Yellow,\n"
      "  Green\n"
      "}\n";

  Lexer lexer(source, "<ncon_coverage>");
  auto tokens = lexer.tokenize();
  if (!lexer.errors().empty()) {
    if (outError != nullptr) {
      *outError = lexer.errors().front();
    }
    return nullptr;
  }

  Parser parser(std::move(tokens), "<ncon_coverage>");
  auto ast = parser.parse();
  if (!parser.errors().empty()) {
    if (outError != nullptr) {
      *outError = parser.errors().front();
    }
    return nullptr;
  }

  SemanticAnalyzer sema;
  sema.analyze(ast.get());
  if (sema.hasErrors()) {
    if (outError != nullptr && !sema.getErrors().empty()) {
      *outError = sema.getErrors().front().toString();
    }
    return nullptr;
  }

  nir::NIRBuilder builder;
  return builder.build(ast.get(), "ncon_coverage");
}

TEST(NconLowerBuildsPortableProgram) {
  nir::Module module("demo");
  auto *init = module.createFunction("Init", NType::makeInt(), false);
  auto *entry = init->createBlock("entry");

  auto add = std::make_unique<nir::Instruction>(nir::InstKind::Add, NType::makeInt(),
                                                "sum");
  add->addOperand(new nir::ConstantInt(40));
  add->addOperand(new nir::ConstantInt(2));
  nir::Instruction *addPtr = add.get();
  entry->addInstruction(std::move(add));

  auto ret =
      std::make_unique<nir::Instruction>(nir::InstKind::Ret, NType::makeInt(), "");
  ret->addOperand(addPtr);
  entry->addInstruction(std::move(ret));

  ncon::Program program;
  std::string error;
  ASSERT_TRUE(ncon::lowerToProgram(module, &program, &error));
  ASSERT_TRUE(error.empty());
  ASSERT_EQ(program.moduleName, "demo");
  ASSERT_EQ(program.functions.size(), 1u);
  ASSERT_EQ(program.blocks.size(), 1u);
  ASSERT_EQ(program.instructions.size(), 2u);
  ASSERT_EQ(program.entryFunctionId, 0u);
  return true;
}

TEST(NconOpcodeCoverageMatchesLanguageFixture) {
  std::string error;
  auto module = buildNconCoverageModule(&error);
  ASSERT_TRUE(module != nullptr);
  ASSERT_TRUE(error.empty());

  for (const auto &fn : module->getFunctions()) {
    for (const auto &block : fn->getBlocks()) {
      for (const auto &inst : block->getInstructions()) {
        ncon::Opcode opcode;
        ASSERT_TRUE(ncon::opcodeFromInstruction(inst->getKind(), &opcode));
      }
    }
  }
  return true;
}

TEST(NconLowerBuildsCoverageProgramFromLanguageFixture) {
  std::string error;
  auto module = buildNconCoverageModule(&error);
  ASSERT_TRUE(module != nullptr);
  ASSERT_TRUE(error.empty());

  ncon::Program program;
  ASSERT_TRUE(ncon::lowerToProgram(*module, &program, &error));
  ASSERT_TRUE(error.empty());
  ASSERT_TRUE(!program.instructions.empty());
  return true;
}

TEST(NconLowerRejectsExternFunctions) {
  nir::Module module("externs");
  module.createFunction("NativeThing", NType::makeVoid(), true);
  module.createFunction("Init", NType::makeVoid(), false)->createBlock("entry");

  ncon::Program program;
  std::string error;
  ASSERT_FALSE(ncon::lowerToProgram(module, &program, &error));
  ASSERT_TRUE(error.find("external native dependencies") != std::string::npos);
  return true;
}

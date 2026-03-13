#include "neuronc/cli/repl/ReplPipeline.h"

#include "neuronc/codegen/JITEngine.h"
#include "neuronc/lexer/Lexer.h"
#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/nir/Optimizer.h"
#include "neuronc/parser/Parser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string_view>

namespace fs = std::filesystem;

namespace neuron::cli {

namespace {

constexpr const char *kReplEntryMethodName = "NeuronReplEntry";

std::string trimCopy(std::string text) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

std::string normalizeModuleName(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

std::string makeVirtualCellPath(std::size_t index) {
  return "<repl:" + std::to_string(index) + ">";
}

std::string normalizeReplCellSourceForParse(std::string source) {
  const std::string trimmed = trimCopy(source);
  if (trimmed.empty() || trimmed.back() == ';') {
    return source;
  }
  source += "\n;";
  return source;
}

fs::path replSettingsAnchorPath() {
  return fs::current_path() / "ReplSession.nr";
}

NeuronSettings replSettingsFrom(const NeuronSettings &settings) {
  NeuronSettings replSettings = settings;
  replSettings.enforceStrictFileNaming = false;
  replSettings.forbidRootScripts = false;
  replSettings.requireScriptDocs = false;
  replSettings.requireScriptDocsMinLines = 0;
  return replSettings;
}

bool containsRestrictedReplSurface(const ASTNode *node, std::string *outReason) {
  if (node == nullptr) {
    return false;
  }

  switch (node->type) {
  case ASTNodeType::ModuleCppDecl:
    if (outReason != nullptr) {
      *outReason = "modulecpp is not supported in the REPL yet";
    }
    return true;
  case ASTNodeType::CanvasStmt:
    if (outReason != nullptr) {
      *outReason = "canvas/graphics lifecycle blocks are not supported in the REPL";
    }
    return true;
  case ASTNodeType::ShaderDecl:
    if (outReason != nullptr) {
      *outReason = "shader declarations are not supported in the REPL";
    }
    return true;
  default:
    break;
  }

  auto checkChildren = [&](const std::vector<ASTNodePtr> &children) {
    for (const auto &child : children) {
      if (containsRestrictedReplSurface(child.get(), outReason)) {
        return true;
      }
    }
    return false;
  };

  switch (node->type) {
  case ASTNodeType::Program:
    return checkChildren(static_cast<const ProgramNode *>(node)->declarations);
  case ASTNodeType::Block:
    return checkChildren(static_cast<const BlockNode *>(node)->statements);
  case ASTNodeType::ClassDecl:
    return checkChildren(static_cast<const ClassDeclNode *>(node)->members);
  case ASTNodeType::CanvasStmt:
    return checkChildren(static_cast<const CanvasStmtNode *>(node)->handlers);
  case ASTNodeType::ShaderDecl:
    return checkChildren(static_cast<const ShaderDeclNode *>(node)->uniforms) ||
           checkChildren(static_cast<const ShaderDeclNode *>(node)->stages);
  default:
    return false;
  }
}

bool isSafeGlobalBinding(const BindingDeclNode *node) {
  if (node == nullptr || node->target != nullptr || node->isAtomic) {
    return false;
  }
  if (node->value == nullptr) {
    return true;
  }

  switch (node->value->type) {
  case ASTNodeType::IntLiteral:
  case ASTNodeType::FloatLiteral:
  case ASTNodeType::StringLiteral:
  case ASTNodeType::BoolLiteral:
  case ASTNodeType::NullLiteral:
    return true;
  default:
    return false;
  }
}

bool isReplaySuppressedPrintCall(const ASTNode *node) {
  if (node == nullptr || node->type != ASTNodeType::CallExpr) {
    return false;
  }

  const auto *callExpr = static_cast<const CallExprNode *>(node);
  if (callExpr->callee == nullptr) {
    return false;
  }

  if (callExpr->callee->type == ASTNodeType::Identifier) {
    return static_cast<const IdentifierNode *>(callExpr->callee.get())->name ==
           "Print";
  }

  if (callExpr->callee->type != ASTNodeType::MemberAccessExpr) {
    return false;
  }

  const auto *memberAccess =
      static_cast<const MemberAccessNode *>(callExpr->callee.get());
  if (memberAccess->member != "Print" || memberAccess->object == nullptr ||
      memberAccess->object->type != ASTNodeType::Identifier) {
    return false;
  }

  return static_cast<const IdentifierNode *>(memberAccess->object.get())->name ==
         "System";
}

std::string replInputGenericTypeName(ASTNode *argNode) {
  if (argNode == nullptr) {
    return "";
  }
  if (argNode->type == ASTNodeType::Identifier) {
    return static_cast<IdentifierNode *>(argNode)->name;
  }
  if (argNode->type == ASTNodeType::TypeSpec) {
    return static_cast<TypeSpecNode *>(argNode)->typeName;
  }
  return "";
}

std::string replResolvedInputTypeName(const InputExprNode *inputExpr) {
  if (inputExpr == nullptr) {
    return "";
  }
  if (inputExpr->typeArguments.empty()) {
    return "string";
  }
  if (inputExpr->typeArguments.size() > 1) {
    return "";
  }
  return replInputGenericTypeName(inputExpr->typeArguments.front().get());
}

bool replInputEchoUsesSecretMode(const InputExprNode *inputExpr) {
  if (inputExpr == nullptr) {
    return false;
  }
  return std::any_of(inputExpr->stages.begin(), inputExpr->stages.end(),
                     [](const InputStageNode &stage) {
                       return stage.method == "Secret";
                     });
}

bool isReplaySuppressedInputExpr(const ASTNode *node) {
  return node != nullptr && node->type == ASTNodeType::InputExpr;
}

std::unique_ptr<ASTNode> makeReplEchoCall(std::string bindingName,
                                          std::string inputTypeName,
                                          SourceLocation location) {
  std::vector<ASTNodePtr> arguments;
  arguments.push_back(
      std::make_unique<IdentifierNode>(std::move(bindingName), location));

  ASTNodePtr callee;
  if (inputTypeName == "string") {
    callee = std::make_unique<IdentifierNode>("__repl_echo_string", location);
  } else {
    callee = std::make_unique<IdentifierNode>("Print", location);
  }

  return std::make_unique<CallExprNode>(std::move(callee), std::move(arguments),
                                        location);
}

std::unique_ptr<ProgramNode> synthesizeProgram(
    std::vector<std::unique_ptr<ProgramNode>> *programs, ReplSubmitResult *result) {
  auto merged = std::make_unique<ProgramNode>(
      SourceLocation{1, 1, "<repl-session>"});
  merged->moduleName = "ReplSession";

  auto entryMethod = std::make_unique<MethodDeclNode>(
      kReplEntryMethodName, SourceLocation{1, 1, "<repl-session>"});
  auto entryBlock = std::make_unique<BlockNode>(
      SourceLocation{1, 1, "<repl-session>"});

  std::unordered_set<std::string> seenModules;
  std::unordered_set<std::string> seenGlobalBindings;

  const std::size_t currentProgramIndex =
      programs->empty() ? 0 : (programs->size() - 1);
  std::size_t replTempValueIndex = 0;
  std::size_t programIndex = 0;
  for (auto &program : *programs) {
    if (program == nullptr) {
      ++programIndex;
      continue;
    }

    const bool isCurrentProgram = programIndex == currentProgramIndex;
    for (auto &decl : program->declarations) {
      if (decl == nullptr) {
        continue;
      }

      std::string restrictionReason;
      if (containsRestrictedReplSurface(decl.get(), &restrictionReason)) {
        if (result != nullptr) {
          result->phase = "semantic";
          result->runtimeError = restrictionReason;
        }
        return nullptr;
      }

      if (decl->type == ASTNodeType::ModuleDecl) {
        const auto *moduleDecl = static_cast<const ModuleDeclNode *>(decl.get());
        const std::string normalized = normalizeModuleName(moduleDecl->moduleName);
        if (!seenModules.insert(normalized).second) {
          continue;
        }
        merged->declarations.push_back(std::move(decl));
        continue;
      }

      if (decl->type == ASTNodeType::MethodDecl) {
        const auto *methodDecl = static_cast<const MethodDeclNode *>(decl.get());
        if (methodDecl->name == kReplEntryMethodName) {
          if (result != nullptr) {
            result->phase = "semantic";
            result->runtimeError =
                "method name '" + std::string(kReplEntryMethodName) +
                "' is reserved by the REPL";
          }
          return nullptr;
        }
        merged->declarations.push_back(std::move(decl));
        continue;
      }

      if (decl->type == ASTNodeType::BindingDecl) {
        auto *bindingDecl = static_cast<BindingDeclNode *>(decl.get());
        if (seenGlobalBindings.find(bindingDecl->name) == seenGlobalBindings.end() &&
            isSafeGlobalBinding(bindingDecl)) {
          seenGlobalBindings.insert(bindingDecl->name);
          merged->declarations.push_back(std::move(decl));
        } else {
          entryBlock->statements.push_back(std::move(decl));
        }
        continue;
      }

      if (decl->type == ASTNodeType::ClassDecl ||
          decl->type == ASTNodeType::EnumDecl) {
        merged->declarations.push_back(std::move(decl));
        continue;
      }

      if (decl->type == ASTNodeType::InputExpr) {
        auto *inputExpr = static_cast<InputExprNode *>(decl.get());
        if (!isCurrentProgram) {
          continue;
        }

        const std::string inputTypeName = replResolvedInputTypeName(inputExpr);
        const bool shouldEcho =
            !inputTypeName.empty() && !replInputEchoUsesSecretMode(inputExpr);
        if (!shouldEcho) {
          entryBlock->statements.push_back(std::move(decl));
          continue;
        }

        const SourceLocation location = decl->location;
        const std::string tempName =
            "replValue" + std::to_string(++replTempValueIndex);
        auto bindingDecl = std::make_unique<BindingDeclNode>(
            tempName, BindingKind::Value, std::move(decl), location);
        entryBlock->statements.push_back(std::move(bindingDecl));
        entryBlock->statements.push_back(
            makeReplEchoCall(tempName, inputTypeName, location));
        continue;
      }

      if (!isCurrentProgram && isReplaySuppressedPrintCall(decl.get())) {
        continue;
      }
      if (!isCurrentProgram && isReplaySuppressedInputExpr(decl.get())) {
        continue;
      }
      entryBlock->statements.push_back(std::move(decl));
    }
    ++programIndex;
  }

  entryMethod->body = std::move(entryBlock);
  merged->declarations.push_back(std::move(entryMethod));
  return merged;
}

bool parsePrograms(const std::vector<ReplCell> &cells,
                   std::vector<std::unique_ptr<ProgramNode>> *outPrograms,
                   ReplSubmitResult *result) {
  if (outPrograms == nullptr) {
    if (result != nullptr) {
      result->phase = "parser";
      result->runtimeError = "internal error: null REPL AST output";
    }
    return false;
  }

  outPrograms->clear();
  outPrograms->reserve(cells.size());

  for (const ReplCell &cell : cells) {
    if (result != nullptr) {
      result->sourceByFile[cell.virtualPath] = cell.source;
    }

    const std::string parseSource = normalizeReplCellSourceForParse(cell.source);
    Lexer lexer(parseSource, cell.virtualPath);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
      if (result != nullptr) {
        result->phase = "lexer";
        result->stringDiagnostics = lexer.errors();
        result->diagnostics = frontend::convertStringDiagnostics(
            "lexer", cell.virtualPath, result->stringDiagnostics);
      }
      return false;
    }

    Parser parser(std::move(tokens), cell.virtualPath);
    auto ast = parser.parse();
    if (!parser.errors().empty()) {
      if (result != nullptr) {
        result->phase = "parser";
        result->stringDiagnostics = parser.errors();
        result->diagnostics =
            frontend::convertParserDiagnostics(cell.virtualPath, parser.diagnostics());
      }
      return false;
    }

    outPrograms->push_back(std::move(ast));
  }

  return true;
}

} // namespace

void ReplSession::reset() { m_cells.clear(); }

void ReplSession::commit(std::string virtualPath, std::string source) {
  m_cells.push_back({std::move(virtualPath), std::move(source)});
}

ReplPipeline::ReplPipeline(ReplPipelineDependencies dependencies)
    : m_deps(std::move(dependencies)) {}

ReplSubmitResult ReplPipeline::submit(ReplSession *session,
                                      const std::string &sourceText) {
  ReplSubmitResult result;
  if (session == nullptr) {
    result.phase = "semantic";
    result.runtimeError = "internal error: null REPL session";
    return result;
  }

  if (trimCopy(sourceText).empty()) {
    return result;
  }

  std::vector<ReplCell> candidateCells = session->cells();
  candidateCells.push_back({makeVirtualCellPath(session->nextCellIndex()), sourceText});

  std::vector<std::unique_ptr<ProgramNode>> parsedPrograms;
  if (!parsePrograms(candidateCells, &parsedPrograms, &result)) {
    return result;
  }

  auto program = synthesizeProgram(&parsedPrograms, &result);
  if (program == nullptr) {
    return result;
  }

  const fs::path sourceAnchor = replSettingsAnchorPath();
  const NeuronSettings replSettings =
      replSettingsFrom(m_deps.loadNeuronSettings(sourceAnchor));
  const auto projectConfig = m_deps.tryLoadProjectConfigFromCwd();

  SemanticAnalyzer sema;
  sema.setAvailableModules(m_deps.collectAvailableModules(sourceAnchor, projectConfig),
                           true);
  sema.setMaxClassesPerFile(replSettings.maxClassesPerFile);
  sema.setRequireMethodUppercaseStart(replSettings.requireMethodUppercaseStart);
  sema.setStrictFileNamingRules(false, sourceAnchor.stem().string());
  sema.setMaxLinesPerMethod(replSettings.maxLinesPerMethod);
  sema.setMaxLinesPerBlockStatement(replSettings.maxLinesPerBlockStatement);
  sema.setMinMethodNameLength(replSettings.minMethodNameLength);
  sema.setRequireClassExplicitVisibility(
      replSettings.requireClassExplicitVisibility);
  sema.setRequirePropertyExplicitVisibility(
      replSettings.requirePropertyExplicitVisibility);
  sema.setRequireConstUppercase(replSettings.requireConstUppercase);
  sema.setMaxNestingDepth(replSettings.maxNestingDepth);
  sema.setRequirePublicMethodDocs(false);
  sema.setSourceText(sourceText);
  sema.setAgentHints(replSettings.agentHints);
  sema.analyze(program.get());
  if (sema.hasErrors()) {
    result.phase = "semantic";
    result.semanticDiagnostics = sema.getErrors();
    result.diagnostics = frontend::convertSemanticDiagnostics(result.semanticDiagnostics);
    return result;
  }

  neuron::BuildOptimizeLevel buildOptLevel = neuron::BuildOptimizeLevel::O0;
  neuron::BuildTargetCPU targetCPU = neuron::BuildTargetCPU::Native;
  if (projectConfig.has_value()) {
    buildOptLevel = projectConfig->optimizeLevel;
    targetCPU = projectConfig->targetCPU;
  }
  m_deps.applyTensorRuntimeEnv(projectConfig);

  neuron::LLVMCodeGenOptions llvmOptions;
  llvmOptions.optLevel = m_deps.toLLVMOptLevel(buildOptLevel);
  llvmOptions.targetCPU = m_deps.toLLVMTargetCPU(targetCPU);

  neuron::nir::NIRBuilder nirBuilder;
  auto nirModule = nirBuilder.build(program.get(), "repl_session");
  if (nirBuilder.hasErrors()) {
    result.phase = "nir";
    result.stringDiagnostics = nirBuilder.errors();
    result.diagnostics =
        frontend::convertStringDiagnostics("nir", "repl_session", result.stringDiagnostics);
    return result;
  }

  auto optimizer = neuron::nir::Optimizer::createDefaultOptimizer();
  optimizer->run(nirModule.get());

  LLVMCodeGen codegen;
  codegen.setEntryFunctionName(kReplEntryMethodName);
  codegen.generate(nirModule.get());

  std::string optError;
  if (!codegen.optimizeModule(llvmOptions, &optError)) {
    result.phase = "jit";
    result.runtimeError = "LLVM optimization failed:\n" + optError;
    return result;
  }

  const fs::path runtimeLibraryPath = m_deps.runtimeSharedLibraryPath();
  if (!fs::exists(runtimeLibraryPath) &&
      !m_deps.ensureRuntimeObjects(llvmOptions)) {
    result.phase = "jit";
    result.runtimeError = "failed to build shared runtime for REPL";
    return result;
  }

  JITEngine jit;
  std::string jitError;
  const bool jitInitialized =
      m_deps.initializeJitEngine
          ? m_deps.initializeJitEngine(&jit, &jitError)
          : jit.initialize(std::make_unique<SharedLibraryJitSymbolProvider>(
                               runtimeLibraryPath),
                           &jitError);
  if (!jitInitialized) {
    result.phase = "jit";
    result.runtimeError = jitError;
    return result;
  }
  if (!jit.addModule(codegen.takeOwnedModule(), &jitError)) {
    result.phase = "jit";
    result.runtimeError = jitError;
    return result;
  }

  int exitCode = 0;
  if (!jit.executeMain(&exitCode, &jitError)) {
    result.phase = "jit";
    result.runtimeError = jitError;
    return result;
  }
  if (exitCode != 0) {
    result.phase = "jit";
    result.runtimeError =
        "JIT execution failed with exit code " + std::to_string(exitCode);
    return result;
  }

  session->commit(candidateCells.back().virtualPath, sourceText);
  result.committed = true;
  return result;
}

bool ReplPipeline::needsContinuation(const std::string &buffer) {
  int parenDepth = 0;
  int braceDepth = 0;
  int bracketDepth = 0;
  bool inString = false;
  bool escapeNext = false;

  for (char ch : buffer) {
    if (inString) {
      if (escapeNext) {
        escapeNext = false;
        continue;
      }
      if (ch == '\\') {
        escapeNext = true;
        continue;
      }
      if (ch == '"') {
        inString = false;
      }
      continue;
    }

    if (ch == '"') {
      inString = true;
      continue;
    }

    switch (ch) {
    case '(':
      ++parenDepth;
      break;
    case ')':
      parenDepth = std::max(0, parenDepth - 1);
      break;
    case '{':
      ++braceDepth;
      break;
    case '}':
      braceDepth = std::max(0, braceDepth - 1);
      break;
    case '[':
      ++bracketDepth;
      break;
    case ']':
      bracketDepth = std::max(0, bracketDepth - 1);
      break;
    default:
      break;
    }
  }

  if (inString || parenDepth > 0 || braceDepth > 0 || bracketDepth > 0) {
    return true;
  }

  const std::string trimmed = trimCopy(buffer);
  if (trimmed.empty()) {
    return false;
  }

  return trimmed.back() == '\\';
}

} // namespace neuron::cli

#include "DeclarationAnalyzer.h"

#include "AnalysisHelpers.h"
#include "SemanticDriver.h"

#include <cctype>
#include <string_view>
#include <unordered_set>

namespace neuron::sema_detail {

namespace {

struct MethodSignatureParts {
  NTypePtr returnType;
  std::vector<NTypePtr> parameterTypes;
  std::vector<std::string> parameterNames;
  std::vector<CallableParameterInfo> signatureParameters;
};

MethodSignatureParts buildMethodSignature(AnalysisContext &context,
                                          MethodDeclNode *node) {
  MethodSignatureParts parts;
  parts.returnType = context.resolveType(node->returnType.get());
  parts.parameterTypes.reserve(node->parameters.size());
  parts.parameterNames.reserve(node->parameters.size());
  parts.signatureParameters.reserve(node->parameters.size());

  for (const auto &param : node->parameters) {
    NTypePtr paramType = context.resolveType(param.typeSpec.get());
    parts.parameterTypes.push_back(paramType);
    parts.parameterNames.push_back(param.name);
    parts.signatureParameters.push_back(
        {param.name, typeDisplayName(paramType)});
  }

  return parts;
}

void registerMethodSymbol(AnalysisContext &context, MethodDeclNode *node,
                          std::string signatureKey, bool isConstructor,
                          bool isPublic) {
  MethodSignatureParts parts = buildMethodSignature(context, node);
  NTypePtr returnType =
      isConstructor ? NType::makeVoid() : std::move(parts.returnType);
  SymbolKind kind =
      isConstructor ? SymbolKind::Constructor : SymbolKind::Method;
  Symbol symbol(node->name, kind,
                NType::makeMethod(returnType, std::move(parts.parameterTypes)));
  symbol.signatureKey = signatureKey;
  symbol.isPublic = isPublic;

  context.defineSymbol(context.currentScope(), node->name, std::move(symbol),
                       &node->location, symbolNameLength(node->name));
  if (!isConstructor) {
    context.registerCallableParamNames(signatureKey,
                                       std::move(parts.parameterNames));
  }
  context.registerCallableSignature(signatureKey,
                                    std::move(parts.signatureParameters),
                                    typeDisplayName(returnType));
}

NTypePtr declarationExposedType(AnalysisContext &context, ASTNode *decl) {
  if (decl == nullptr) {
    return NType::makeUnknown();
  }
  switch (decl->type) {
  case ASTNodeType::MethodDecl: {
    auto *method = static_cast<MethodDeclNode *>(decl);
    MethodSignatureParts parts = buildMethodSignature(context, method);
    return NType::makeMethod(parts.returnType, std::move(parts.parameterTypes));
  }
  case ASTNodeType::ClassDecl: {
    auto *cls = static_cast<ClassDeclNode *>(decl);
    return NType::makeClass(cls->name);
  }
  case ASTNodeType::EnumDecl: {
    auto *enumDecl = static_cast<EnumDeclNode *>(decl);
    return NType::makeEnum(enumDecl->name);
  }
  default:
    return NType::makeUnknown();
  }
}

std::string declarationName(ASTNode *decl) {
  if (decl == nullptr) {
    return {};
  }
  switch (decl->type) {
  case ASTNodeType::MethodDecl:
    return static_cast<MethodDeclNode *>(decl)->name;
  case ASTNodeType::ClassDecl:
    return static_cast<ClassDeclNode *>(decl)->name;
  case ASTNodeType::EnumDecl:
    return static_cast<EnumDeclNode *>(decl)->name;
  default:
    return {};
  }
}

SymbolKind declarationSymbolKind(ASTNode *decl) {
  if (decl == nullptr) {
    return SymbolKind::Variable;
  }
  switch (decl->type) {
  case ASTNodeType::MethodDecl:
    return SymbolKind::Method;
  case ASTNodeType::ClassDecl:
    return SymbolKind::Class;
  case ASTNodeType::EnumDecl:
    return SymbolKind::Enum;
  default:
    return SymbolKind::Variable;
  }
}

std::string moduleNameFromFilePath(std::string_view filePath) {
  if (filePath.empty()) {
    return {};
  }

  std::size_t end = filePath.size();
  while (end > 0 &&
         (filePath[end - 1] == '/' || filePath[end - 1] == '\\')) {
    --end;
  }
  std::size_t start = end;
  while (start > 0 && filePath[start - 1] != '/' &&
         filePath[start - 1] != '\\') {
    --start;
  }

  std::string fileName(filePath.substr(start, end - start));
  const std::size_t dot = fileName.rfind('.');
  if (dot != std::string::npos) {
    fileName.resize(dot);
  }
  return fileName;
}

} // namespace

DeclarationAnalyzer::DeclarationAnalyzer(SemanticDriver &driver)
    : m_driver(driver) {}

void DeclarationAnalyzer::analyze(const ProgramView &program) {
  AnalysisContext &context = m_driver.context();
  context.reset();
  m_driver.flow().reset();
  context.setDocumentDeclarations(program.declarations);

  if (program.declarations.empty()) {
    return;
  }

  context.recordScopeSnapshot(program.location);

  int classCount = 0;
  SourceLocation firstClassLoc;
  std::string firstClassName;
  for (ASTNode *decl : program.declarations) {
    if (decl == nullptr || decl->type != ASTNodeType::ClassDecl) {
      continue;
    }
    ++classCount;
    if (classCount == 1) {
      auto *cls = static_cast<ClassDeclNode *>(decl);
      firstClassLoc = cls->location;
      firstClassName = cls->name;
    }
  }

  const AnalysisOptions &options = context.options();
  if (options.enforceStrictFileNamingRules && !options.sourceFileStem.empty()) {
    const unsigned char firstChar =
        static_cast<unsigned char>(options.sourceFileStem.front());
    if (std::islower(firstChar)) {
      context.error(program.location,
                    "Invalid .nr filename '" + options.sourceFileStem +
                        ".nr': filename must start with an uppercase letter.");
    }
    if (options.sourceFileStem.find('_') != std::string::npos) {
      context.error(program.location,
                    "Invalid .nr filename '" + options.sourceFileStem +
                        ".nr': filename cannot include '_'.");
    }
    if (classCount == 1 && firstClassName != options.sourceFileStem) {
      context.error(firstClassLoc,
                    "Class name must match module filename. Expected: " +
                        options.sourceFileStem + " Found: " + firstClassName);
    }
  }

  if (options.maxClassesPerFile > 0) {
    int classCountForLimit = 0;
    SourceLocation extraClassLoc;
    for (ASTNode *decl : program.declarations) {
      if (decl != nullptr && decl->type == ASTNodeType::ClassDecl &&
          ++classCountForLimit == options.maxClassesPerFile + 1) {
        extraClassLoc = decl->location;
      }
    }
    if (classCountForLimit > options.maxClassesPerFile) {
      if (options.maxClassesPerFile == 1) {
        context.error(extraClassLoc,
                      "Multiple classes defined in module. Each .nr file may "
                      "contain only one class.");
      } else {
        context.error(extraClassLoc,
                      "Too many classes defined in module. Allowed: " +
                          std::to_string(options.maxClassesPerFile) +
                          ", found: " +
                          std::to_string(classCountForLimit) + ".");
      }
    }
  }

  registerGlobalDeclarations(program.declarations);
  visitDeclarations(program.declarations);
}

void DeclarationAnalyzer::visitProgram(ProgramNode *node) {
  if (node == nullptr) {
    return;
  }

  std::vector<ASTNode *> declarations;
  declarations.reserve(node->declarations.size());
  for (auto &decl : node->declarations) {
    declarations.push_back(decl.get());
  }
  visitDeclarations(declarations);
}

void DeclarationAnalyzer::visitDeclarations(
    const std::vector<ASTNode *> &declarations) {
  for (ASTNode *decl : declarations) {
    if (decl == nullptr) {
      continue;
    }

    switch (decl->type) {
    case ASTNodeType::ModuleDecl:
      visitModuleDecl(static_cast<ModuleDeclNode *>(decl));
      break;
    case ASTNodeType::ExpandModuleDecl:
      visitExpandModuleDecl(static_cast<ExpandModuleDeclNode *>(decl));
      break;
    case ASTNodeType::ClassDecl:
      visitClassDecl(static_cast<ClassDeclNode *>(decl));
      break;
    case ASTNodeType::EnumDecl:
      visitEnumDecl(static_cast<EnumDeclNode *>(decl));
      break;
    case ASTNodeType::MethodDecl:
      visitMethodDecl(static_cast<MethodDeclNode *>(decl));
      break;
    case ASTNodeType::ExternDecl: {
      auto *externDecl = static_cast<ExternDeclNode *>(decl);
      if (externDecl->declaration != nullptr &&
          externDecl->declaration->type == ASTNodeType::MethodDecl) {
        auto *methodDecl =
            static_cast<MethodDeclNode *>(externDecl->declaration.get());
        methodDecl->isExtern = true;
        if (externDecl->symbolOverride.has_value()) {
          methodDecl->externSymbolOverride = externDecl->symbolOverride;
        }
        visitMethodDecl(methodDecl);
      }
      break;
    }
    case ASTNodeType::BindingDecl:
      m_driver.bindings().visit(static_cast<BindingDeclNode *>(decl));
      break;
    case ASTNodeType::CastStmt:
      m_driver.statements().visitCastStmt(static_cast<CastStmtNode *>(decl));
      break;
    case ASTNodeType::MacroDecl:
      m_driver.statements().visitMacroDecl(static_cast<MacroDeclNode *>(decl));
      break;
    case ASTNodeType::StaticAssertStmt:
      m_driver.statements().visitStaticAssertStmt(
          static_cast<StaticAssertStmtNode *>(decl));
      break;
    case ASTNodeType::GpuBlock:
      m_driver.graphics().visitGpuBlock(static_cast<GpuBlockNode *>(decl));
      break;
    case ASTNodeType::CanvasStmt:
      m_driver.graphics().visitCanvasStmt(static_cast<CanvasStmtNode *>(decl));
      break;
    case ASTNodeType::ShaderDecl:
      m_driver.graphics().visitShaderDecl(static_cast<ShaderDeclNode *>(decl));
      break;
    default:
      break;
    }
  }
}

void DeclarationAnalyzer::visitModuleDecl(ModuleDeclNode *node) {
  if (node == nullptr) {
    return;
  }

  AnalysisContext &context = m_driver.context();
  const AnalysisOptions &options = context.options();
  if (options.enforceStrictFileNamingRules && !options.sourceFileStem.empty() &&
      normalizeModuleName(node->moduleName) ==
          normalizeModuleName(options.sourceFileStem)) {
    context.error(node->location,
                  "Module '" + node->moduleName +
                      "' cannot import itself. Use 'module' only for external "
                      "modules.");
    return;
  }

  if (context.types().moduleResolutionEnabled() &&
      !context.types().isModuleAvailable(node->moduleName)) {
    const std::string normalized = normalizeModuleName(node->moduleName);
    context.error(node->location,
                  "Unknown module: " + node->moduleName +
                      ". Install via 'neuron add " + normalized +
                      "' or provide modules/" + node->moduleName + ".nr");
    return;
  }

  context.defineSymbol(context.globalScope(), node->moduleName,
                       Symbol(node->moduleName, SymbolKind::Module,
                              NType::makeModule(node->moduleName)),
                       &node->location, symbolNameLength(node->moduleName));
}

void DeclarationAnalyzer::visitExpandModuleDecl(ExpandModuleDeclNode *node) {
  if (node == nullptr) {
    return;
  }

  AnalysisContext &context = m_driver.context();
  if (context.types().moduleResolutionEnabled() &&
      !context.types().isModuleAvailable(node->moduleName)) {
    const std::string normalized = normalizeModuleName(node->moduleName);
    context.error(node->location,
                  "Unknown module: " + node->moduleName +
                      ". Install via 'neuron add " + normalized +
                      "' or provide modules/" + node->moduleName + ".nr");
    return;
  }

  context.defineSymbol(context.globalScope(), node->moduleName,
                       Symbol(node->moduleName, SymbolKind::Module,
                              NType::makeModule(node->moduleName)),
                       &node->location, symbolNameLength(node->moduleName));

  const auto declarations = context.references().documentDeclarations();
  for (ASTNode *decl : declarations) {
    if (decl == nullptr || decl == node) {
      continue;
    }
    const std::string name = declarationName(decl);
    if (name.empty()) {
      continue;
    }
    NTypePtr type = declarationExposedType(context, decl);
    if (type == nullptr || type->isUnknown() || type->isError()) {
      continue;
    }
    context.types().registerModuleMember(node->moduleName, name, type,
                                         declarationSymbolKind(decl),
                                         decl->location, true);

    Symbol symbol(name, declarationSymbolKind(decl), type);
    symbol.definition = SymbolLocation{decl->location, symbolNameLength(name)};
    context.defineSymbol(context.globalScope(), name, std::move(symbol),
                         &decl->location, symbolNameLength(name));
  }
}


void DeclarationAnalyzer::visitClassDecl(ClassDeclNode *node) {
  if (node == nullptr) {
    return;
  }

  AnalysisContext &context = m_driver.context();
  if (context.options().requireClassExplicitVisibility &&
      node->access == AccessModifier::None) {
    context.errorWithAgentHint(
        node->location,
        "Class '" + node->name +
            "' must explicitly declare visibility (public/private).",
        "Declare class visibility explicitly: `public " + node->name +
            " is class { ... }`.");
  }

  const TypeResolver::ClassRecord *classRecord =
      context.types().findClass(node->name);
  if (classRecord == nullptr) {
    return;
  }

  std::unordered_set<std::string> seenBases;
  for (const auto &baseName : classRecord->baseClasses) {
    if (baseName == node->name) {
      context.error(node->location,
                    "Class '" + node->name + "' cannot inherit itself");
      continue;
    }
    if (!seenBases.insert(baseName).second) {
      context.error(node->location,
                    "Duplicate base class in inherits list: " + baseName);
    }
  }

  AnalysisContext::ScopeHandle oldScope = context.currentScope();
  context.setCurrentScope({&context, classRecord->scopeId});
  m_driver.flow().enterScope();

  if (Symbol *thisSymbol =
          context.defineSymbol(context.currentScope(), "this",
                               Symbol("this", SymbolKind::Parameter,
                                      classRecord->type))) {
    m_driver.flow().declareSymbol(thisSymbol, true, nullptr, classRecord->type);
  }
  for (const auto &param : node->genericParams) {
    context.defineSymbol(context.currentScope(), param,
                         Symbol(param, SymbolKind::GenericParameter,
                                NType::makeGeneric(param)));
  }
  context.recordScopeSnapshot(node->location);

  for (auto &member : node->members) {
    if (member->type == ASTNodeType::BindingDecl) {
      auto *field = static_cast<BindingDeclNode *>(member.get());
      if (node->kind == ClassKind::Interface) {
        context.error(field->location,
                      "Interface cannot declare fields: " + field->name);
        continue;
      }
      if (context.options().requirePropertyExplicitVisibility &&
          field->access == AccessModifier::None) {
        context.errorWithAgentHint(
            field->location,
            "Class member '" + node->name + "." + field->name +
                "' must explicitly declare visibility (public/private).",
            "Add `public` or `private` before class fields and methods.");
      }
      m_driver.rules().validateVariableName(field->name, field->location,
                                            field->isConst);
      NTypePtr fieldType = context.resolveType(field->typeAnnotation.get());
      if (fieldType->isAuto() && field->value) {
        fieldType = m_driver.inferExpression(field->value.get());
      }
      Symbol fieldSymbol(field->name, SymbolKind::Field, fieldType);
      fieldSymbol.isConst = field->isConst;
      fieldSymbol.isMutable = !field->isConst;
      fieldSymbol.isPublic = (field->access == AccessModifier::Public);
      if (Symbol *definedField =
              context.defineSymbol(context.currentScope(), field->name,
                                   std::move(fieldSymbol), &field->location,
                                   symbolNameLength(field->name))) {
        m_driver.flow().declareSymbol(definedField, field->value != nullptr,
                                      field->value.get(), fieldType);
      }
      continue;
    }

    if (member->type != ASTNodeType::MethodDecl) {
      continue;
    }

    auto *method = static_cast<MethodDeclNode *>(member.get());
    if (node->kind == ClassKind::Interface) {
      method->isAbstract = true;
    }
    if (context.options().requirePropertyExplicitVisibility &&
        method->access == AccessModifier::None) {
      context.errorWithAgentHint(
          method->location,
          "Class member '" + node->name + "." + method->name +
              "' must explicitly declare visibility (public/private).",
          "Add `public` or `private` before class fields and methods.");
    }

    const bool isConstructor = method->name == "constructor";
    const std::string signatureKey =
        isConstructor ? node->name + ".constructor"
                      : node->name + "." + method->name;
    registerMethodSymbol(context, method, signatureKey, isConstructor,
                         method->access == AccessModifier::Public);
  }

  for (auto &member : node->members) {
    if (member->type == ASTNodeType::MethodDecl) {
      visitMethodDecl(static_cast<MethodDeclNode *>(member.get()));
    }
  }

  context.setCurrentScope(oldScope);
  m_driver.flow().leaveScope();
  if (!node->members.empty()) {
    context.recordScopeSnapshot(nodeEndLocation(node->members.back().get()));
  } else {
    context.recordScopeSnapshot(node->location);
  }
}

void DeclarationAnalyzer::visitEnumDecl(EnumDeclNode *node) {
  if (node != nullptr && node->members.empty()) {
    m_driver.context().error(
        node->location, "Enum '" + node->name + "' must have at least one member");
  }
}

void DeclarationAnalyzer::visitMethodDecl(MethodDeclNode *node) {
  if (node == nullptr) {
    return;
  }

  AnalysisContext &context = m_driver.context();
  std::vector<NTypePtr> methodParamTypes;
  methodParamTypes.reserve(node->parameters.size());
  for (const auto &param : node->parameters) {
    methodParamTypes.push_back(context.resolveType(param.typeSpec.get()));
  }
  context.rememberType(node, NType::makeMethod(context.resolveType(node->returnType.get()),
                                               methodParamTypes));
  m_driver.rules().validateMethodName(node->name, node->location);

  if (node->isAbstract && node->body) {
    context.error(node->location,
                  "Abstract method cannot declare a body: " + node->name);
  }

  if (context.options().requirePublicMethodDocs &&
      node->access == AccessModifier::Public && !node->isExtern &&
      !m_driver.rules().hasRequiredPublicMethodDocs(node)) {
    context.errorWithAgentHint(
        node->location,
        "Public method '" + node->name +
            "' must have XML documentation comments with <summary>.",
        "Add C#-style docs directly above the method, e.g. `/// "
        "<summary>...</summary>`.");
  }

  if (context.options().maxLinesPerMethod > 0 && node->body != nullptr &&
      node->body->type == ASTNodeType::Block) {
    auto *methodBody = static_cast<BlockNode *>(node->body.get());
    if (methodBody->endLine >= node->location.line) {
      const int methodLines = methodBody->endLine - node->location.line + 1;
      if (methodLines > context.options().maxLinesPerMethod) {
        context.errorWithAgentHint(
            node->location,
            "Method '" + node->name + "' exceeds maximum allowed length (" +
                std::to_string(methodLines) + " lines, limit " +
                std::to_string(context.options().maxLinesPerMethod) + ").",
            "Split large methods into smaller helpers.");
      }
    }
  }

  if (node->isExtern) {
    // Extern method: type contract is established.
    // Symbol resolution (toml lookup vs inline override) happens at codegen time.
    // Here we validate that signature matches the native provider's contract.
    // TODO: signature validation against native.toml exports
    return;
  }

  const sema_detail::FlowAnalyzer::Snapshot outerFlowState =
      m_driver.flow().snapshot();
  context.enterScope(node->name);
  m_driver.flow().enterScope();
  for (const auto &param : node->parameters) {
    NTypePtr paramType = context.resolveType(param.typeSpec.get());
    if (Symbol *paramSymbol = context.defineSymbol(
            context.currentScope(), param.name,
            Symbol(param.name, SymbolKind::Parameter, paramType),
            &param.location, symbolNameLength(param.name))) {
      m_driver.flow().declareSymbol(paramSymbol, true, nullptr, paramType);
    }
  }

  if (node->body != nullptr) {
    context.recordScopeSnapshot(node->body->location);
    if (node->body->type == ASTNodeType::Block) {
      m_driver.visitBlock(static_cast<BlockNode *>(node->body.get()));
    } else {
      m_driver.inferExpression(node->body.get());
    }
  }

  m_driver.flow().leaveScope();
  m_driver.flow().restore(outerFlowState);
  context.leaveScope();
  if (node->body != nullptr) {
    context.recordScopeSnapshot(nodeEndLocation(node));
  }
}

void DeclarationAnalyzer::registerGlobalDeclarations(
    const std::vector<ASTNode *> &declarations) {
  AnalysisContext &context = m_driver.context();
  std::string currentModuleName;
  std::string currentFile;
  for (ASTNode *decl : declarations) {
    if (decl == nullptr) {
      continue;
    }

    if (decl->location.file != currentFile) {
      currentFile = decl->location.file;
      currentModuleName.clear();
    }

    if (decl->type == ASTNodeType::ModuleDecl) {
      currentModuleName = static_cast<ModuleDeclNode *>(decl)->moduleName;
      continue;
    }
    if (decl->type == ASTNodeType::ExpandModuleDecl) {
      currentModuleName = static_cast<ExpandModuleDeclNode *>(decl)->moduleName;
      continue;
    }

    std::string moduleForDecl = currentModuleName;
    if (moduleForDecl.empty()) {
      moduleForDecl = moduleNameFromFilePath(decl->location.file);
    }

    if (decl->type == ASTNodeType::ClassDecl) {
      auto *cls = static_cast<ClassDeclNode *>(decl);
      SymbolTable::ScopeId scopeId =
          context.symbols().createScope(context.globalScope().id(), cls->name);
      context.types().registerClass(cls->name,
                                    cls->access == AccessModifier::Public,
                                    cls->baseClasses, scopeId);
      context.defineSymbol(context.globalScope(), cls->name,
                           Symbol(cls->name, SymbolKind::Class,
                                  NType::makeClass(cls->name)),
                           &cls->location, symbolNameLength(cls->name));
      if (!moduleForDecl.empty()) {
        context.types().registerModuleMember(moduleForDecl, cls->name,
                                             NType::makeClass(cls->name),
                                             SymbolKind::Class, cls->location);
      }
      continue;
    }

    if (decl->type == ASTNodeType::MethodDecl) {
      auto *method = static_cast<MethodDeclNode *>(decl);
      registerMethodSymbol(context, method, method->name, false,
                           method->access == AccessModifier::Public);
      MethodSignatureParts parts = buildMethodSignature(context, method);
      NTypePtr methodType =
          NType::makeMethod(parts.returnType, parts.parameterTypes);
      context.rememberType(method, methodType);
      if (!moduleForDecl.empty()) {
        context.types().registerModuleMember(moduleForDecl, method->name,
                                             methodType, SymbolKind::Method,
                                             method->location);
      }
      continue;
    }

    if (decl->type == ASTNodeType::ExternDecl) {
      auto *externDecl = static_cast<ExternDeclNode *>(decl);
      if (externDecl->declaration == nullptr ||
          externDecl->declaration->type != ASTNodeType::MethodDecl) {
        continue;
      }
      auto *method = static_cast<MethodDeclNode *>(externDecl->declaration.get());
      method->isExtern = true;
      if (externDecl->symbolOverride.has_value()) {
        method->externSymbolOverride = externDecl->symbolOverride;
      }
      registerMethodSymbol(context, method, method->name, false,
                           method->access == AccessModifier::Public);
      MethodSignatureParts parts = buildMethodSignature(context, method);
      NTypePtr methodType =
          NType::makeMethod(parts.returnType, parts.parameterTypes);
      context.rememberType(method, methodType);
      if (!moduleForDecl.empty()) {
        context.types().registerModuleMember(moduleForDecl, method->name,
                                             methodType, SymbolKind::Method,
                                             method->location);
      }
      continue;
    }

    if (decl->type == ASTNodeType::EnumDecl) {
      auto *enumDecl = static_cast<EnumDeclNode *>(decl);
      context.types().registerEnum(
          enumDecl->name,
          std::unordered_set<std::string>(enumDecl->members.begin(),
                                          enumDecl->members.end()));
      NTypePtr enumType = NType::makeEnum(enumDecl->name);
      context.defineSymbol(context.globalScope(), enumDecl->name,
                           Symbol(enumDecl->name, SymbolKind::Enum, enumType),
                           &enumDecl->location,
                           symbolNameLength(enumDecl->name));
      context.rememberType(enumDecl, enumType);
      if (!moduleForDecl.empty()) {
        context.types().registerModuleMember(moduleForDecl, enumDecl->name,
                                             enumType, SymbolKind::Enum,
                                             enumDecl->location);
      }
      continue;
    }

    if (decl->type == ASTNodeType::ShaderDecl) {
      auto *shaderDecl = static_cast<ShaderDeclNode *>(decl);
      context.defineSymbol(context.globalScope(), shaderDecl->name,
                           Symbol(shaderDecl->name, SymbolKind::Shader,
                                  NType::makeClass("Shader")),
                           &shaderDecl->location,
                           symbolNameLength(shaderDecl->name));
      context.rememberType(shaderDecl, NType::makeClass("Shader"));
    }
  }
}

} // namespace neuron::sema_detail

#include "AnalysisContext.h"

#include "AnalysisHelpers.h"

#include <unordered_set>

namespace neuron::sema_detail {

namespace {

std::string materialScopeKey(SymbolTable::ScopeId scopeId,
                             const std::string &name) {
  return std::to_string(static_cast<unsigned long long>(scopeId)) + ":" + name;
}

std::string scopedGraphicsKey(SymbolTable::ScopeId scopeId,
                              const std::string &name) {
  return std::to_string(static_cast<unsigned long long>(scopeId)) + ":" + name;
}

void registerBuiltinClass(AnalysisContext &context, const std::string &name) {
  SymbolTable::ScopeId scopeId =
      context.symbols().createScope(context.globalScope().id(), name);
  context.types().registerClass(name, true, {}, scopeId);
  context.defineSymbol(context.globalScope(), name,
                       Symbol(name, SymbolKind::Class, NType::makeClass(name)));
}

void registerBuiltinClassMethod(
    AnalysisContext &context, const std::string &className,
    const std::string &methodName, NTypePtr returnType,
    std::vector<NTypePtr> parameterTypes,
    std::vector<CallableParameterInfo> signatureParameters) {
  const auto *classRecord = context.types().findClass(className);
  if (classRecord == nullptr ||
      classRecord->scopeId == SymbolTable::kInvalidScope) {
    return;
  }

  const std::string signatureKey = className + "." + methodName;
  std::vector<std::string> parameterNames;
  parameterNames.reserve(signatureParameters.size());
  for (const auto &parameter : signatureParameters) {
    parameterNames.push_back(parameter.name);
  }

  Symbol methodSymbol(methodName, SymbolKind::Method,
                      NType::makeMethod(returnType, std::move(parameterTypes)));
  methodSymbol.signatureKey = signatureKey;
  context.defineSymbol({&context, classRecord->scopeId}, methodName,
                       std::move(methodSymbol));
  context.registerCallableParamNames(signatureKey, std::move(parameterNames));
  context.registerCallableSignature(signatureKey,
                                    std::move(signatureParameters),
                                    typeDisplayName(returnType));
}

void registerBuiltinConstructorSignature(
    AnalysisContext &context, const std::string &className,
    std::vector<CallableParameterInfo> signatureParameters) {
  const std::string signatureKey = className + ".constructor";
  context.registerCallableSignature(signatureKey, std::move(signatureParameters),
                                    className);
}

void declareBuiltinGraphicsClasses(AnalysisContext &context) {
  for (const char *className :
       {"Window", "Shader", "Material", "Mesh", "Texture2D", "Sampler",
        "Color", "Vector2", "Vector3", "Vector4", "Matrix4", "CommandList",
        "Scene", "Entity", "Transform", "Renderer2D", "Camera2D",
        "SpriteRenderer2D", "ShapeRenderer2D", "TextRenderer2D", "Font"}) {
    registerBuiltinClass(context, className);
  }

  registerBuiltinClassMethod(
      context, "Window", "Create", NType::makeClass("Window"),
      {NType::makeInt(), NType::makeInt(), NType::makeString()},
      {{"width", "int"}, {"height", "int"}, {"title", "string"}});
  registerBuiltinClassMethod(context, "Scene", "Create", NType::makeClass("Scene"),
                             {}, {});
  registerBuiltinClassMethod(
      context, "Scene", "CreateEntity", NType::makeClass("Entity"),
      {NType::makeString()}, {{"name", "string"}});
  registerBuiltinClassMethod(context, "Scene", "DestroyEntity",
                             NType::makeVoid(), {NType::makeClass("Entity")},
                             {{"entity", "Entity"}});
  registerBuiltinClassMethod(
      context, "Scene", "FindEntity", NType::makeClass("Entity"),
      {NType::makeString()}, {{"name", "string"}});
  registerBuiltinClassMethod(context, "Entity", "GetTransform",
                             NType::makeClass("Transform"), {}, {});
  registerBuiltinClassMethod(context, "Entity", "AddCamera2D",
                             NType::makeClass("Camera2D"), {}, {});
  registerBuiltinClassMethod(context, "Entity", "AddSpriteRenderer2D",
                             NType::makeClass("SpriteRenderer2D"), {}, {});
  registerBuiltinClassMethod(context, "Entity", "AddShapeRenderer2D",
                             NType::makeClass("ShapeRenderer2D"), {}, {});
  registerBuiltinClassMethod(context, "Entity", "AddTextRenderer2D",
                             NType::makeClass("TextRenderer2D"), {}, {});
  registerBuiltinClassMethod(
      context, "Transform", "SetParent", NType::makeVoid(),
      {NType::makeClass("Entity")}, {{"parent", "Entity"}});
  registerBuiltinClassMethod(
      context, "Transform", "SetPosition", NType::makeVoid(),
      {NType::makeClass("Vector3")}, {{"value", "Vector3"}});
  registerBuiltinClassMethod(
      context, "Transform", "SetRotation", NType::makeVoid(),
      {NType::makeClass("Vector3")}, {{"value", "Vector3"}});
  registerBuiltinClassMethod(
      context, "Transform", "SetScale", NType::makeVoid(),
      {NType::makeClass("Vector3")}, {{"value", "Vector3"}});
  registerBuiltinClassMethod(context, "Renderer2D", "Create",
                             NType::makeClass("Renderer2D"), {}, {});
  registerBuiltinClassMethod(context, "Renderer2D", "SetClearColor",
                             NType::makeVoid(), {NType::makeClass("Color")},
                             {{"color", "Color"}});
  registerBuiltinClassMethod(context, "Renderer2D", "SetCamera",
                             NType::makeVoid(),
                             {NType::makeClass("Camera2D")},
                             {{"camera", "Camera2D"}});
  registerBuiltinClassMethod(context, "Renderer2D", "Render",
                             NType::makeVoid(), {NType::makeClass("Scene")},
                             {{"scene", "Scene"}});
  registerBuiltinClassMethod(context, "Camera2D", "SetZoom",
                             NType::makeVoid(), {NType::makeFloat()},
                             {{"value", "float"}});
  registerBuiltinClassMethod(context, "Camera2D", "SetPrimary",
                             NType::makeVoid(), {NType::makeBool()},
                             {{"value", "bool"}});
  registerBuiltinClassMethod(context, "Mesh", "Load", NType::makeClass("Mesh"),
                             {NType::makeString()}, {{"path", "string"}});
  registerBuiltinClassMethod(context, "SpriteRenderer2D", "SetTexture",
                             NType::makeVoid(),
                             {NType::makeClass("Texture2D")},
                             {{"texture", "Texture2D"}});
  registerBuiltinClassMethod(context, "SpriteRenderer2D", "SetColor",
                             NType::makeVoid(), {NType::makeClass("Color")},
                             {{"color", "Color"}});
  registerBuiltinClassMethod(context, "SpriteRenderer2D", "SetSize",
                             NType::makeVoid(), {NType::makeClass("Vector2")},
                             {{"size", "Vector2"}});
  registerBuiltinClassMethod(context, "SpriteRenderer2D", "SetPivot",
                             NType::makeVoid(), {NType::makeClass("Vector2")},
                             {{"pivot", "Vector2"}});
  registerBuiltinClassMethod(context, "SpriteRenderer2D", "SetFlipX",
                             NType::makeVoid(), {NType::makeBool()},
                             {{"value", "bool"}});
  registerBuiltinClassMethod(context, "SpriteRenderer2D", "SetFlipY",
                             NType::makeVoid(), {NType::makeBool()},
                             {{"value", "bool"}});
  registerBuiltinClassMethod(context, "SpriteRenderer2D", "SetSortingLayer",
                             NType::makeVoid(), {NType::makeInt()},
                             {{"value", "int"}});
  registerBuiltinClassMethod(context, "SpriteRenderer2D", "SetOrderInLayer",
                             NType::makeVoid(), {NType::makeInt()},
                             {{"value", "int"}});
  registerBuiltinClassMethod(context, "ShapeRenderer2D", "SetRectangle",
                             NType::makeVoid(), {NType::makeClass("Vector2")},
                             {{"size", "Vector2"}});
  registerBuiltinClassMethod(context, "ShapeRenderer2D", "SetCircle",
                             NType::makeVoid(),
                             {NType::makeFloat(), NType::makeInt()},
                             {{"radius", "float"}, {"segments", "int"}});
  registerBuiltinClassMethod(
      context, "ShapeRenderer2D", "SetLine", NType::makeVoid(),
      {NType::makeClass("Vector2"), NType::makeClass("Vector2"),
       NType::makeFloat()},
      {{"start", "Vector2"}, {"end", "Vector2"}, {"thickness", "float"}});
  registerBuiltinClassMethod(context, "ShapeRenderer2D", "SetColor",
                             NType::makeVoid(), {NType::makeClass("Color")},
                             {{"color", "Color"}});
  registerBuiltinClassMethod(context, "ShapeRenderer2D", "SetFilled",
                             NType::makeVoid(), {NType::makeBool()},
                             {{"value", "bool"}});
  registerBuiltinClassMethod(context, "ShapeRenderer2D", "SetSortingLayer",
                             NType::makeVoid(), {NType::makeInt()},
                             {{"value", "int"}});
  registerBuiltinClassMethod(context, "ShapeRenderer2D", "SetOrderInLayer",
                             NType::makeVoid(), {NType::makeInt()},
                             {{"value", "int"}});
  registerBuiltinClassMethod(context, "Texture2D", "Load",
                             NType::makeClass("Texture2D"),
                             {NType::makeString()}, {{"path", "string"}});
  registerBuiltinClassMethod(
      context, "Texture2D", "Sample", NType::makeClass("Color"),
      {NType::makeClass("Sampler"), NType::makeClass("Vector2")},
      {{"sampler", "Sampler"}, {"uv", "Vector2"}});
  registerBuiltinClassMethod(context, "Sampler", "Create",
                             NType::makeClass("Sampler"), {},
                             {});
  registerBuiltinClassMethod(context, "Font", "Load", NType::makeClass("Font"),
                             {NType::makeString()}, {{"path", "string"}});
  registerBuiltinClassMethod(context, "TextRenderer2D", "SetFont",
                             NType::makeVoid(), {NType::makeClass("Font")},
                             {{"font", "Font"}});
  registerBuiltinClassMethod(context, "TextRenderer2D", "SetText",
                             NType::makeVoid(), {NType::makeString()},
                             {{"value", "string"}});
  registerBuiltinClassMethod(context, "TextRenderer2D", "SetFontSize",
                             NType::makeVoid(), {NType::makeFloat()},
                             {{"value", "float"}});
  registerBuiltinClassMethod(context, "TextRenderer2D", "SetColor",
                             NType::makeVoid(), {NType::makeClass("Color")},
                             {{"color", "Color"}});
  registerBuiltinClassMethod(context, "TextRenderer2D", "SetAlignment",
                             NType::makeVoid(), {NType::makeInt()},
                             {{"value", "int"}});
  registerBuiltinClassMethod(context, "TextRenderer2D", "SetSortingLayer",
                             NType::makeVoid(), {NType::makeInt()},
                             {{"value", "int"}});
  registerBuiltinClassMethod(context, "TextRenderer2D", "SetOrderInLayer",
                             NType::makeVoid(), {NType::makeInt()},
                             {{"value", "int"}});
  registerBuiltinClassMethod(context, "CommandList", "Clear",
                             NType::makeVoid(), {NType::makeClass("Color")},
                             {{"color", "Color"}});
  registerBuiltinClassMethod(
      context, "CommandList", "Draw", NType::makeVoid(),
      {NType::makeClass("Mesh"), NType::makeClass("Material")},
      {{"mesh", "Mesh"}, {"material", "Material"}});
  registerBuiltinClassMethod(
      context, "CommandList", "DrawIndexed", NType::makeVoid(),
      {NType::makeClass("Mesh"), NType::makeClass("Material")},
      {{"mesh", "Mesh"}, {"material", "Material"}});
  registerBuiltinClassMethod(
      context, "CommandList", "DrawInstanced", NType::makeVoid(),
      {NType::makeClass("Mesh"), NType::makeClass("Material"),
       NType::makeInt()},
      {{"mesh", "Mesh"}, {"material", "Material"}, {"instances", "int"}});

  registerBuiltinConstructorSignature(
      context, "Color",
      {{"red", "float"}, {"green", "float"}, {"blue", "float"},
       {"alpha", "float"}});
  registerBuiltinConstructorSignature(
      context, "Vector2", {{"x", "float"}, {"y", "float"}});
  registerBuiltinConstructorSignature(
      context, "Vector3", {{"x", "float"}, {"y", "float"}, {"z", "float"}});
  registerBuiltinConstructorSignature(
      context, "Vector4",
      {{"x", "float"}, {"y", "float"}, {"z", "float"}, {"w", "float"}});
}

} // namespace

AnalysisContext::ScopeHandle::ScopeHandle(AnalysisContext *owner,
                                          SymbolTable::ScopeId scopeId)
    : m_owner(owner), m_scopeId(scopeId) {}

const AnalysisContext::ScopeHandle *AnalysisContext::ScopeHandle::operator->()
    const {
  return this;
}

AnalysisContext::ScopeHandle *AnalysisContext::ScopeHandle::operator->() {
  return this;
}

Symbol *AnalysisContext::ScopeHandle::lookup(const std::string &name) const {
  return m_owner != nullptr ? m_owner->m_symbols.lookupVisible(m_scopeId, name)
                            : nullptr;
}

Symbol *AnalysisContext::ScopeHandle::lookupLocal(const std::string &name) const {
  return m_owner != nullptr ? m_owner->m_symbols.lookupLocal(m_scopeId, name)
                            : nullptr;
}

AnalysisContext::ScopeHandle AnalysisContext::ScopeHandle::parent() const {
  if (m_owner == nullptr) {
    return {};
  }
  return {m_owner, m_owner->m_symbols.parentScope(m_scopeId)};
}

AnalysisContext::ScopeHandle::operator bool() const {
  return m_owner != nullptr && m_scopeId != SymbolTable::kInvalidScope;
}

bool AnalysisContext::ScopeHandle::operator==(const ScopeHandle &other) const {
  return m_scopeId == other.m_scopeId;
}

bool AnalysisContext::ScopeHandle::operator!=(const ScopeHandle &other) const {
  return !(*this == other);
}

SymbolTable::ScopeId AnalysisContext::ScopeHandle::id() const {
  return m_scopeId;
}

AnalysisContext::AnalysisContext(const AnalysisOptions &options,
                                 SymbolTable &symbols, TypeResolver &types,
                                 ReferenceTracker &references,
                                 ScopeManager &scopes,
                                 DiagnosticEmitter &diagnostics)
    : m_options(options), m_symbols(symbols), m_types(types),
      m_references(references), m_scopes(scopes), m_diagnostics(diagnostics) {}

void AnalysisContext::reset() {
  m_diagnostics.reset();
  m_references.reset();
  m_scopes.reset();
  m_symbols.reset();
  m_types.reset();
  m_types.declareBuiltinTypes();

  m_globalScope = makeScopeHandle(m_symbols.globalScope());
  m_currentScope = makeScopeHandle(m_symbols.currentScope());
  m_controlDepth = 0;
  m_graphicsFrameDepth = 0;
  m_shaderBindings.clear();
  m_materialShaders.clear();
  m_entityBindings.clear();
  m_transformEntities.clear();

  declareBuiltinGraphicsClasses(*this);
  declareBuiltinFunctions();
}

void AnalysisContext::setDocumentDeclarations(
    const std::vector<ASTNode *> &declarations) {
  m_references.setDocumentDeclarations(declarations);
}

const AnalysisOptions &AnalysisContext::options() const { return m_options; }

SymbolTable &AnalysisContext::symbols() { return m_symbols; }

const SymbolTable &AnalysisContext::symbols() const { return m_symbols; }

TypeResolver &AnalysisContext::types() { return m_types; }

const TypeResolver &AnalysisContext::types() const { return m_types; }

ReferenceTracker &AnalysisContext::references() { return m_references; }

ScopeManager &AnalysisContext::scopes() { return m_scopes; }

DiagnosticEmitter &AnalysisContext::diagnostics() { return m_diagnostics; }

const DiagnosticEmitter &AnalysisContext::diagnostics() const {
  return m_diagnostics;
}

AnalysisContext::ScopeHandle AnalysisContext::globalScope() const {
  return m_globalScope;
}

AnalysisContext::ScopeHandle AnalysisContext::currentScope() const {
  return m_currentScope;
}

void AnalysisContext::setCurrentScope(const ScopeHandle &scope) {
  m_currentScope = scope;
  if (scope) {
    m_symbols.setCurrentScope(scope.id());
  }
}

void AnalysisContext::enterScope(const std::string &name) {
  m_symbols.pushScope(name);
  setCurrentScope(makeScopeHandle(m_symbols.currentScope()));
}

void AnalysisContext::leaveScope() {
  m_symbols.popScope();
  setCurrentScope(makeScopeHandle(m_symbols.currentScope()));
}

int AnalysisContext::enterControl() { return ++m_controlDepth; }

void AnalysisContext::leaveControl() {
  if (m_controlDepth > 0) {
    --m_controlDepth;
  }
}

int AnalysisContext::controlDepth() const { return m_controlDepth; }

void AnalysisContext::enterGraphicsFrame() { ++m_graphicsFrameDepth; }

void AnalysisContext::leaveGraphicsFrame() {
  if (m_graphicsFrameDepth > 0) {
    --m_graphicsFrameDepth;
  }
}

bool AnalysisContext::isInGraphicsFrame() const {
  return m_graphicsFrameDepth > 0;
}

NTypePtr AnalysisContext::resolveType(ASTNode *node) {
  return m_types.resolveType(node, m_symbols, m_references, m_diagnostics);
}

NTypePtr AnalysisContext::resolveType(const std::string &name) {
  return m_types.resolveType(name, m_symbols);
}

NTypePtr AnalysisContext::rememberType(const ASTNode *node, NTypePtr type) {
  return m_references.rememberType(node, std::move(type));
}

void AnalysisContext::recordScopeSnapshot(const SourceLocation &location) {
  if (!m_currentScope || location.file.empty()) {
    return;
  }
  m_scopes.recordSnapshot(location,
                          m_symbols.snapshotVisibleSymbols(m_currentScope.id()));
}

Symbol *AnalysisContext::defineSymbol(const ScopeHandle &scope,
                                      const std::string &name, Symbol symbol,
                                      const SourceLocation *definitionLocation,
                                      int definitionLength) {
  if (!scope) {
    return nullptr;
  }

  Symbol *defined = m_symbols.defineInScope(scope.id(), name, std::move(symbol));
  if (defined != nullptr && definitionLocation != nullptr &&
      !definitionLocation->file.empty()) {
    m_references.recordDefinition(defined, *definitionLocation, definitionLength);
    recordScopeSnapshot(*definitionLocation);
  }
  return defined;
}

void AnalysisContext::recordReference(Symbol *symbol, const SourceLocation &loc,
                                      int length) {
  m_references.recordReference(symbol, loc, length);
}

void AnalysisContext::error(const SourceLocation &loc,
                            const std::string &message) {
  m_diagnostics.emit(loc, message);
}

void AnalysisContext::error(const SourceLocation &loc, const std::string &code,
                            diagnostics::DiagnosticArguments arguments,
                            const std::string &message) {
  m_diagnostics.emit(loc, code, std::move(arguments), message);
}

void AnalysisContext::errorWithAgentHint(const SourceLocation &loc,
                                         const std::string &message,
                                         std::string_view hint) {
  m_diagnostics.emitWithAgentHint(loc, message, hint);
}

void AnalysisContext::registerCallableParamNames(
    const std::string &callableName, std::vector<std::string> parameterNames) {
  m_types.registerCallableParamNames(callableName, std::move(parameterNames));
}

void AnalysisContext::registerCallableSignature(
    const std::string &callableKey,
    std::vector<CallableParameterInfo> parameters,
    const std::string &returnType) {
  m_types.registerCallableSignature(callableKey, std::move(parameters),
                                    returnType);
}

void AnalysisContext::registerShaderBinding(const std::string &shaderName,
                                            const std::string &bindingName,
                                            const std::string &typeName) {
  if (shaderName.empty() || bindingName.empty()) {
    return;
  }
  m_shaderBindings[shaderName][bindingName] = ShaderBindingInfo{typeName};
}

const AnalysisContext::ShaderBindingInfo *
AnalysisContext::findShaderBinding(const std::string &shaderName,
                                   const std::string &bindingName) const {
  const auto shaderIt = m_shaderBindings.find(shaderName);
  if (shaderIt == m_shaderBindings.end()) {
    return nullptr;
  }
  const auto bindingIt = shaderIt->second.find(bindingName);
  if (bindingIt == shaderIt->second.end()) {
    return nullptr;
  }
  return &bindingIt->second;
}

void AnalysisContext::registerMaterialShader(const ScopeHandle &scope,
                                             const std::string &materialName,
                                             const std::string &shaderName) {
  if (!scope || materialName.empty() || shaderName.empty()) {
    return;
  }
  m_materialShaders[materialScopeKey(scope.id(), materialName)] = shaderName;
}

std::string AnalysisContext::findMaterialShader(
    const ScopeHandle &scope, const std::string &materialName) const {
  if (!scope || materialName.empty()) {
    return {};
  }

  ScopeHandle cursor = scope;
  while (cursor) {
    const auto it =
        m_materialShaders.find(materialScopeKey(cursor.id(), materialName));
    if (it != m_materialShaders.end()) {
      return it->second;
    }
    cursor = cursor.parent();
  }
  return {};
}

void AnalysisContext::registerEntityBinding(const ScopeHandle &scope,
                                            const std::string &entityName) {
  if (!scope || entityName.empty()) {
    return;
  }
  m_entityBindings.try_emplace(scopedGraphicsKey(scope.id(), entityName));
}

bool AnalysisContext::registerEntityComponent(const ScopeHandle &scope,
                                              const std::string &entityName,
                                              const std::string &componentName) {
  if (!scope || entityName.empty() || componentName.empty()) {
    return false;
  }
  EntityBindingInfo &info =
      m_entityBindings[scopedGraphicsKey(scope.id(), entityName)];
  return info.components.emplace(componentName, true).second;
}

bool AnalysisContext::hasEntityComponent(const ScopeHandle &scope,
                                         const std::string &entityName,
                                         const std::string &componentName) const {
  if (!scope || entityName.empty() || componentName.empty()) {
    return false;
  }
  const auto entityIt =
      m_entityBindings.find(scopedGraphicsKey(scope.id(), entityName));
  if (entityIt == m_entityBindings.end()) {
    return false;
  }
  return entityIt->second.components.find(componentName) !=
         entityIt->second.components.end();
}

void AnalysisContext::registerTransformBinding(const ScopeHandle &scope,
                                               const std::string &transformName,
                                               const std::string &entityName) {
  if (!scope || transformName.empty() || entityName.empty()) {
    return;
  }
  m_transformEntities[scopedGraphicsKey(scope.id(), transformName)] = entityName;
}

std::string AnalysisContext::findTransformEntity(
    const ScopeHandle &scope, const std::string &transformName) const {
  if (!scope || transformName.empty()) {
    return {};
  }
  const auto it =
      m_transformEntities.find(scopedGraphicsKey(scope.id(), transformName));
  if (it == m_transformEntities.end()) {
    return {};
  }
  return it->second;
}

bool AnalysisContext::setEntityParent(const ScopeHandle &scope,
                                      const std::string &entityName,
                                      const std::string &parentEntityName) {
  std::unordered_set<std::string> seen;
  std::string cursorName = parentEntityName;
  if (!scope || entityName.empty()) {
    return false;
  }

  while (!cursorName.empty()) {
    if (!seen.insert(cursorName).second || cursorName == entityName) {
      return false;
    }
    const auto parentIt =
        m_entityBindings.find(scopedGraphicsKey(scope.id(), cursorName));
    if (parentIt == m_entityBindings.end()) {
      break;
    }
    cursorName = parentIt->second.parentEntityName;
  }

  m_entityBindings[scopedGraphicsKey(scope.id(), entityName)].parentEntityName =
      parentEntityName;
  return true;
}

const std::vector<std::string> *AnalysisContext::findNamedCallableSignature(
    const std::string &callableName, std::size_t argumentCount,
    const SourceLocation &loc) {
  const auto match = m_types.findNamedCallableSignature(callableName, argumentCount);
  switch (match.status) {
  case TypeResolver::CallableParamMatchStatus::Ok:
    return match.parameters;
  case TypeResolver::CallableParamMatchStatus::MissingCallable:
    error(loc, "Named arguments require a known method signature for '" +
                   callableName + "'");
    return nullptr;
  case TypeResolver::CallableParamMatchStatus::NoArityMatch:
    error(loc, "No overload of '" + callableName + "' accepts " +
                   std::to_string(argumentCount) +
                   " argument(s) for named argument binding");
    return nullptr;
  case TypeResolver::CallableParamMatchStatus::Ambiguous:
    error(loc, "Named argument binding for '" + callableName +
                   "' is ambiguous with " + std::to_string(argumentCount) +
                   " argument(s)");
    return nullptr;
  }

  return nullptr;
}

void AnalysisContext::declareBuiltinFunctions() {
  Symbol printSymbol("Print", SymbolKind::Method,
                     NType::makeMethod(NType::makeVoid(), {NType::makeString()}));
  printSymbol.signatureKey = "Print";
  defineSymbol(m_globalScope, "Print", std::move(printSymbol));
  registerCallableParamNames("Print", {"value"});
  registerCallableSignature("Print", {{"value", "string"}}, "void");

  Symbol inputSymbol(
      "Input", SymbolKind::Method,
      NType::makeMethod(NType::makeDynamic(), {NType::makeString()}));
  inputSymbol.signatureKey = "Input";
  defineSymbol(m_globalScope, "Input", std::move(inputSymbol));
  registerCallableParamNames("Input", {"prompt"});
  registerCallableSignature("Input", {{"prompt", "string"}}, "dynamic");

  Symbol replEchoSymbol("__repl_echo_string", SymbolKind::Method,
                        NType::makeMethod(NType::makeVoid(),
                                          {NType::makeString()}));
  replEchoSymbol.signatureKey = "__repl_echo_string";
  defineSymbol(m_globalScope, "__repl_echo_string", std::move(replEchoSymbol));
  registerCallableParamNames("__repl_echo_string", {"value"});
  registerCallableSignature("__repl_echo_string", {{"value", "string"}},
                            "void");

  Symbol divideSymbol("Divide", SymbolKind::Method,
                      NType::makeMethod(NType::makeFloat(),
                                        {NType::makeFloat(), NType::makeFloat()}));
  divideSymbol.signatureKey = "Divide";
  defineSymbol(m_globalScope, "Divide", std::move(divideSymbol));
  registerCallableParamNames("Divide", {"left", "right"});
  registerCallableSignature("Divide",
                            {{"left", "float"}, {"right", "float"}}, "float");
}

AnalysisContext::ScopeHandle
AnalysisContext::makeScopeHandle(SymbolTable::ScopeId scopeId) const {
  return {const_cast<AnalysisContext *>(this), scopeId};
}

} // namespace neuron::sema_detail

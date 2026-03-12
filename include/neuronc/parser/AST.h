#pragma once
#include "neuronc/lexer/Token.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>


namespace neuron {

// ── Forward declarations ──
struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

// ── AST node types ──
enum class ASTNodeType {
  // Program structure
  Program,
  ModuleDecl,
  ExpandModuleDecl,
  ModuleCppDecl,

  // Declarations
  BindingDecl,     // x is 10;  /  y is another x;
  MethodDecl,
  ClassDecl,
  EnumDecl,
  FieldDecl,
  ConstructorDecl,
  ExternDecl,

  // Expressions
  IntLiteral,
  FloatLiteral,
  StringLiteral,
  BoolLiteral,
  NullLiteral,
  Identifier,
  Error,
  BinaryExpr,
  UnaryExpr,
  CallExpr,
  InputExpr,
  MemberAccessExpr,
  IndexExpr,
  LambdaExpr,
  AddressOfExpr,
  ValueOfExpr,
  AnotherExpr,
  MoveExpr,
  SliceExpr,
  TypeCastExpr,

  // Control flow
  IfStmt,
  MatchStmt,
  MatchArm,
  WhileStmt,
  ForStmt,
  ForInStmt,
  ParallelForStmt,
  BreakStmt,
  ContinueStmt,
  ReturnStmt,

  // Error handling
  TryStmt,
  CatchClause,
  ThrowStmt,

  // Async
  AsyncMethodDecl,
  AwaitExpr,

  // Concurrency
  AtomicStmt,
  MatchExpr,

  // Safety
  UnsafeBlock,
  GpuBlock,
  CanvasStmt,
  CanvasEventHandler,
  ShaderDecl,
  ShaderStage,
  ShaderPassStmt,

  // Metaprogramming
  MacroDecl,
  StaticAssertStmt,
  TypeofExpr,

  // Type specifications
  TypeSpec,
  GenericTypeSpec,

  // Block
  Block,

  // Assignment update (x is x + 1 when x already exists)
  AssignStmt,
  CastStmt,

  // Increment/Decrement
  IncrementStmt,
  DecrementStmt,
};

// ── Binding kind ──
enum class BindingKind {
  Alias,     // y is x;
  Copy,      // y is another x;
  Value,     // x is 10;
  AddressOf, // p is address of x;
  ValueOf,   // Print(value of p);
  MoveFrom,  // b is move a;
};

// ── Access modifier ──
enum class AccessModifier {
  None,
  Public,
  Private,
};

enum class ClassKind {
  Class,
  Struct,
  Interface,
};

enum class CanvasEventKind {
  OnOpen,
  OnFrame,
  OnResize,
  OnClose,
  Unknown,
};

enum class ShaderStageKind {
  Vertex,
  Fragment,
};

// ── AST Node base ──
struct ASTNode {
  ASTNodeType type;
  SourceLocation location;

  virtual ~ASTNode() = default;

protected:
  ASTNode(ASTNodeType t, SourceLocation loc)
      : type(t), location(std::move(loc)) {}
};

// ── Program node ──
struct ProgramNode : ASTNode {
  std::string moduleName;
  std::vector<ASTNodePtr> declarations;
  ProgramNode(SourceLocation loc) : ASTNode(ASTNodeType::Program, loc) {}
};

// ── Module declaration ──
struct ModuleDeclNode : ASTNode {
  std::string moduleName;
  ModuleDeclNode(std::string name, SourceLocation loc)
      : ASTNode(ASTNodeType::ModuleDecl, loc), moduleName(std::move(name)) {}
};

struct ExpandModuleDeclNode : ASTNode {
  std::string moduleName;
  ExpandModuleDeclNode(std::string name, SourceLocation loc)
      : ASTNode(ASTNodeType::ExpandModuleDecl, loc),
        moduleName(std::move(name)) {}
};

struct ModuleCppDeclNode : ASTNode {
  std::string moduleName;
  ModuleCppDeclNode(std::string name, SourceLocation loc)
      : ASTNode(ASTNodeType::ModuleCppDecl, loc),
        moduleName(std::move(name)) {}
};

// ── Literal nodes ──
struct IntLiteralNode : ASTNode {
  int64_t value;
  IntLiteralNode(int64_t v, SourceLocation loc)
      : ASTNode(ASTNodeType::IntLiteral, loc), value(v) {}
};

struct FloatLiteralNode : ASTNode {
  double value;
  FloatLiteralNode(double v, SourceLocation loc)
      : ASTNode(ASTNodeType::FloatLiteral, loc), value(v) {}
};

struct StringLiteralNode : ASTNode {
  std::string value;
  StringLiteralNode(std::string v, SourceLocation loc)
      : ASTNode(ASTNodeType::StringLiteral, loc), value(std::move(v)) {}
};

struct BoolLiteralNode : ASTNode {
  bool value;
  BoolLiteralNode(bool v, SourceLocation loc)
      : ASTNode(ASTNodeType::BoolLiteral, loc), value(v) {}
};

struct NullLiteralNode : ASTNode {
  NullLiteralNode(SourceLocation loc)
      : ASTNode(ASTNodeType::NullLiteral, loc) {}
};

// ── Identifier ──
struct IdentifierNode : ASTNode {
  std::string name;
  IdentifierNode(std::string n, SourceLocation loc)
      : ASTNode(ASTNodeType::Identifier, loc), name(std::move(n)) {}
};

struct ErrorNode : ASTNode {
  std::string message;
  ErrorNode(std::string text, SourceLocation loc)
      : ASTNode(ASTNodeType::Error, loc), message(std::move(text)) {}
};

// ── Type specification ──
struct TypeSpecNode : ASTNode {
  std::string typeName; // "int", "float", "Vector2", "Array", "Tensor"
  std::vector<ASTNodePtr> genericArgs; // <T>, <float, 3, 3>
  TypeSpecNode(std::string name, SourceLocation loc)
      : ASTNode(ASTNodeType::TypeSpec, loc), typeName(std::move(name)) {}
};

struct CastStepNode {
  ASTNodePtr typeSpec;
  bool allowNullOnFailure = false;
  SourceLocation location;

  CastStepNode(ASTNodePtr typeSpecNode, bool nullable, SourceLocation loc)
      : typeSpec(std::move(typeSpecNode)), allowNullOnFailure(nullable),
        location(std::move(loc)) {}
};

// ── Binding declaration: x is 10; y is another x; p is address of x; ──
struct BindingDeclNode : ASTNode {
  std::string name;
  AccessModifier access;
  BindingKind kind;
  ASTNodePtr value;  // right-hand side
  ASTNodePtr target; // left-hand side if not simple identifier (e.g. this.x)
  ASTNodePtr typeAnnotation; // optional 'as Type'
  bool isConst;
  bool isAtomic;
  BindingDeclNode(std::string n, BindingKind k, ASTNodePtr val,
                  SourceLocation loc)
      : ASTNode(ASTNodeType::BindingDecl, loc), name(std::move(n)),
        access(AccessModifier::None), kind(k), value(std::move(val)),
        target(nullptr), isConst(false), isAtomic(false) {}
};

// ── Binary expression ──
struct BinaryExprNode : ASTNode {
  TokenType op;
  ASTNodePtr left;
  ASTNodePtr right;
  BinaryExprNode(TokenType o, ASTNodePtr l, ASTNodePtr r, SourceLocation loc)
      : ASTNode(ASTNodeType::BinaryExpr, loc), op(o), left(std::move(l)),
        right(std::move(r)) {}
};

// ── Unary expression ──
struct UnaryExprNode : ASTNode {
  TokenType op;
  ASTNodePtr operand;
  bool isPrefix;
  UnaryExprNode(TokenType o, ASTNodePtr oper, bool prefix, SourceLocation loc)
      : ASTNode(ASTNodeType::UnaryExpr, loc), op(o), operand(std::move(oper)),
        isPrefix(prefix) {}
};

// ── Call expression ──
struct CallExprNode : ASTNode {
  ASTNodePtr callee;
  std::vector<ASTNodePtr> arguments;
  std::vector<std::string> argumentLabels;
  bool isFusionChain = false;
  std::vector<std::string> fusionCallNames;
  CallExprNode(ASTNodePtr c, std::vector<ASTNodePtr> args, SourceLocation loc)
      : ASTNode(ASTNodeType::CallExpr, loc), callee(std::move(c)),
        arguments(std::move(args)), argumentLabels(arguments.size()) {}
  CallExprNode(ASTNodePtr c, std::vector<ASTNodePtr> args,
               std::vector<std::string> labels, SourceLocation loc)
      : ASTNode(ASTNodeType::CallExpr, loc), callee(std::move(c)),
        arguments(std::move(args)), argumentLabels(std::move(labels)) {
    if (argumentLabels.size() != arguments.size()) {
      argumentLabels.assign(arguments.size(), "");
    }
  }
};

struct InputStageNode {
  std::string method;
  std::vector<ASTNodePtr> arguments;
  SourceLocation location;

  InputStageNode(std::string methodName, std::vector<ASTNodePtr> args,
                 SourceLocation loc)
      : method(std::move(methodName)), arguments(std::move(args)),
        location(std::move(loc)) {}
};

struct InputExprNode : ASTNode {
  std::vector<ASTNodePtr> typeArguments;
  std::vector<ASTNodePtr> promptArguments;
  std::vector<InputStageNode> stages;

  InputExprNode(SourceLocation loc) : ASTNode(ASTNodeType::InputExpr, loc) {}
};

// ── Member access (a.b) ──
struct MemberAccessNode : ASTNode {
  ASTNodePtr object;
  std::string member;
  SourceLocation memberLocation;
  MemberAccessNode(ASTNodePtr obj, std::string mem, SourceLocation loc,
                   SourceLocation memberLoc)
      : ASTNode(ASTNodeType::MemberAccessExpr, loc), object(std::move(obj)),
        member(std::move(mem)), memberLocation(std::move(memberLoc)) {}
};

// ── Index expression (a[i]) ──
struct IndexExprNode : ASTNode {
  ASTNodePtr object;
  std::vector<ASTNodePtr> indices;
  IndexExprNode(ASTNodePtr obj, std::vector<ASTNodePtr> idx, SourceLocation loc)
      : ASTNode(ASTNodeType::IndexExpr, loc), object(std::move(obj)),
        indices(std::move(idx)) {}
};

// ── Slice expression (a[start..end]) ──
struct SliceExprNode : ASTNode {
  ASTNodePtr object;
  ASTNodePtr start;
  ASTNodePtr end;
  SliceExprNode(ASTNodePtr obj, ASTNodePtr s, ASTNodePtr e, SourceLocation loc)
      : ASTNode(ASTNodeType::SliceExpr, loc), object(std::move(obj)),
        start(std::move(s)), end(std::move(e)) {}
};

// -- typeof(expression) --
struct TypeofExprNode : ASTNode {
  ASTNodePtr expression;
  TypeofExprNode(ASTNodePtr expr, SourceLocation loc)
      : ASTNode(ASTNodeType::TypeofExpr, loc), expression(std::move(expr)) {}
};

// ── Method declaration ──
struct ParameterNode {
  std::string name;
  SourceLocation location;
  ASTNodePtr typeSpec;
};

struct MethodDeclNode : ASTNode {
  std::string name;
  AccessModifier access;
  std::vector<ParameterNode> parameters;
  ASTNodePtr returnType; // optional
  ASTNodePtr body;       // Block
  bool isAsync;
  bool isConstexpr;
  bool isExtern;
  bool isAbstract;
  bool isVirtual;
  bool isOverride;
  bool isOverload;
  std::vector<std::string> genericParams; // <T>
  std::optional<std::string> externSymbolOverride;

  MethodDeclNode(std::string n, SourceLocation loc)
      : ASTNode(ASTNodeType::MethodDecl, loc), name(std::move(n)),
        access(AccessModifier::None), isAsync(false), isConstexpr(false),
        isExtern(false), isAbstract(false), isVirtual(false),
        isOverride(false), isOverload(false) {}
};

struct ExternDeclNode : ASTNode {
  ASTNodePtr declaration;
  std::optional<std::string> symbolOverride;

  ExternDeclNode(ASTNodePtr decl, std::optional<std::string> symbol,
                 SourceLocation loc)
      : ASTNode(ASTNodeType::ExternDecl, loc), declaration(std::move(decl)),
        symbolOverride(std::move(symbol)) {}
};

// ── Class declaration ──
struct ClassDeclNode : ASTNode {
  std::string name;
  AccessModifier access;
  ClassKind kind;
  bool isAbstract;
  std::vector<std::string> baseClasses; // inherits
  std::vector<ASTNodePtr> members;      // fields + methods + constructors
  std::vector<std::string> genericParams;

  ClassDeclNode(std::string n, SourceLocation loc)
      : ASTNode(ASTNodeType::ClassDecl, loc), name(std::move(n)),
        access(AccessModifier::None), kind(ClassKind::Class),
        isAbstract(false) {}
};

struct EnumDeclNode : ASTNode {
  std::string name;
  AccessModifier access;
  std::vector<std::string> members;

  EnumDeclNode(std::string enumName, SourceLocation loc)
      : ASTNode(ASTNodeType::EnumDecl, loc), name(std::move(enumName)),
        access(AccessModifier::None) {}
};

// ── Block ──
struct BlockNode : ASTNode {
  std::vector<ASTNodePtr> statements;
  int endLine;
  int endColumn;
  bool hasExplicitBraces;
  BlockNode(SourceLocation loc)
      : ASTNode(ASTNodeType::Block, loc), endLine(loc.line),
        endColumn(loc.column), hasExplicitBraces(true) {}
};

// ── If statement ──
struct IfStmtNode : ASTNode {
  ASTNodePtr condition;
  ASTNodePtr thenBlock;
  ASTNodePtr elseBlock; // optional — could be another IfStmt
  IfStmtNode(SourceLocation loc) : ASTNode(ASTNodeType::IfStmt, loc) {}
};

// ── While statement ──
struct MatchArmNode : ASTNode {
  std::vector<ASTNodePtr> patternExprs; // empty for default
  ASTNodePtr body;                      // Block for statement form
  ASTNodePtr valueExpr;                 // Expression for expression form
  bool isDefault = false;

  MatchArmNode(std::vector<ASTNodePtr> patterns, ASTNodePtr caseBody,
               ASTNodePtr caseValue, bool defaultCase, SourceLocation loc)
      : ASTNode(ASTNodeType::MatchArm, loc),
        patternExprs(std::move(patterns)), body(std::move(caseBody)),
        valueExpr(std::move(caseValue)), isDefault(defaultCase) {}
};

struct MatchStmtNode : ASTNode {
  std::vector<ASTNodePtr> expressions;
  std::vector<ASTNodePtr> arms;
  MatchStmtNode(SourceLocation loc) : ASTNode(ASTNodeType::MatchStmt, loc) {}
};

struct MatchExprNode : ASTNode {
  std::vector<ASTNodePtr> expressions;
  std::vector<ASTNodePtr> arms;
  MatchExprNode(SourceLocation loc) : ASTNode(ASTNodeType::MatchExpr, loc) {}
};

struct WhileStmtNode : ASTNode {
  ASTNodePtr condition;
  ASTNodePtr body;
  WhileStmtNode(SourceLocation loc) : ASTNode(ASTNodeType::WhileStmt, loc) {}
};

// ── For statement ──
struct ForStmtNode : ASTNode {
  ASTNodePtr init;
  ASTNodePtr condition;
  ASTNodePtr increment;
  ASTNodePtr body;
  bool isParallel;
  ForStmtNode(SourceLocation loc)
      : ASTNode(ASTNodeType::ForStmt, loc), isParallel(false) {}
};

// ── For-in statement ──
struct ForInStmtNode : ASTNode {
  std::string variable;
  SourceLocation variableLocation;
  ASTNodePtr iterable;
  ASTNodePtr body;
  ForInStmtNode(SourceLocation loc) : ASTNode(ASTNodeType::ForInStmt, loc) {}
};

// ── Return ──
struct ReturnStmtNode : ASTNode {
  ASTNodePtr value; // optional
  ReturnStmtNode(SourceLocation loc) : ASTNode(ASTNodeType::ReturnStmt, loc) {}
};

// ── Try / Catch / Finally ──
struct CastStmtNode : ASTNode {
  ASTNodePtr target;
  std::vector<CastStepNode> steps;
  bool pipelineNullable = false;

  CastStmtNode(ASTNodePtr targetExpr, SourceLocation loc)
      : ASTNode(ASTNodeType::CastStmt, loc), target(std::move(targetExpr)) {}
};

struct CatchClauseNode : ASTNode {
  std::string errorName;
  SourceLocation errorLocation;
  ASTNodePtr errorType; // optional specific type
  ASTNodePtr body;
  CatchClauseNode(SourceLocation loc)
      : ASTNode(ASTNodeType::CatchClause, loc) {}
};

struct TryStmtNode : ASTNode {
  ASTNodePtr tryBlock;
  std::vector<ASTNodePtr> catchClauses;
  ASTNodePtr finallyBlock; // optional
  TryStmtNode(SourceLocation loc) : ASTNode(ASTNodeType::TryStmt, loc) {}
};

// ── Throw ──
struct ThrowStmtNode : ASTNode {
  ASTNodePtr value;
  ThrowStmtNode(SourceLocation loc) : ASTNode(ASTNodeType::ThrowStmt, loc) {}
};

struct MacroDeclNode : ASTNode {
  std::string name;
  MacroDeclNode(std::string macroName, SourceLocation loc)
      : ASTNode(ASTNodeType::MacroDecl, loc), name(std::move(macroName)) {}
};

struct StaticAssertStmtNode : ASTNode {
  ASTNodePtr condition;
  std::string message;
  StaticAssertStmtNode(SourceLocation loc)
      : ASTNode(ASTNodeType::StaticAssertStmt, loc) {}
};

// ── Break / Continue ──
struct BreakStmtNode : ASTNode {
  BreakStmtNode(SourceLocation loc) : ASTNode(ASTNodeType::BreakStmt, loc) {}
};

struct ContinueStmtNode : ASTNode {
  ContinueStmtNode(SourceLocation loc)
      : ASTNode(ASTNodeType::ContinueStmt, loc) {}
};

// ── Increment/Decrement ──
struct IncrementStmtNode : ASTNode {
  std::string variable;
  IncrementStmtNode(std::string var, SourceLocation loc)
      : ASTNode(ASTNodeType::IncrementStmt, loc), variable(std::move(var)) {}
};

struct DecrementStmtNode : ASTNode {
  std::string variable;
  DecrementStmtNode(std::string var, SourceLocation loc)
      : ASTNode(ASTNodeType::DecrementStmt, loc), variable(std::move(var)) {}
};

// ── Unsafe block ──
struct UnsafeBlockNode : ASTNode {
  ASTNodePtr body;
  UnsafeBlockNode(SourceLocation loc)
      : ASTNode(ASTNodeType::UnsafeBlock, loc) {}
};

enum class GpuDevicePreferenceMode {
  Default = 0,
  Prefer = 1,
  Force = 2,
};

enum class GpuDevicePreferenceTarget {
  Any = 0,
  Discrete = 1,
  Integrated = 2,
};

struct GpuBlockNode : ASTNode {
  GpuDevicePreferenceMode preferenceMode = GpuDevicePreferenceMode::Default;
  GpuDevicePreferenceTarget preferenceTarget = GpuDevicePreferenceTarget::Any;
  ASTNodePtr body;
  GpuBlockNode(SourceLocation loc) : ASTNode(ASTNodeType::GpuBlock, loc) {}
};

struct CanvasEventHandlerNode : ASTNode {
  CanvasEventKind eventKind = CanvasEventKind::Unknown;
  std::string eventName;
  std::string externalMethodName;
  ASTNodePtr handlerMethod; // MethodDecl for inline handlers.
  bool isExternalBinding = false;

  CanvasEventHandlerNode(CanvasEventKind kind, std::string name,
                         SourceLocation loc)
      : ASTNode(ASTNodeType::CanvasEventHandler, loc), eventKind(kind),
        eventName(std::move(name)) {}
};

struct CanvasStmtNode : ASTNode {
  ASTNodePtr windowExpr;
  std::vector<ASTNodePtr> handlers; // CanvasEventHandler nodes
  CanvasStmtNode(SourceLocation loc) : ASTNode(ASTNodeType::CanvasStmt, loc) {}
};

struct ShaderPassStmtNode : ASTNode {
  std::string varyingName;
  ShaderPassStmtNode(std::string name, SourceLocation loc)
      : ASTNode(ASTNodeType::ShaderPassStmt, loc),
        varyingName(std::move(name)) {}
};

struct ShaderStageNode : ASTNode {
  ShaderStageKind stageKind = ShaderStageKind::Vertex;
  ASTNodePtr methodDecl; // MethodDecl
  ShaderStageNode(ShaderStageKind kind, SourceLocation loc)
      : ASTNode(ASTNodeType::ShaderStage, loc), stageKind(kind) {}
};

struct ShaderDeclNode : ASTNode {
  std::string name;
  AccessModifier access = AccessModifier::None;
  std::vector<ASTNodePtr> uniforms; // BindingDecl nodes
  std::vector<ASTNodePtr> stages;   // ShaderStage nodes
  std::vector<ASTNodePtr> methods;  // MethodDecl nodes (CPU-side descriptor methods)

  ShaderDeclNode(std::string shaderName, SourceLocation loc)
      : ASTNode(ASTNodeType::ShaderDecl, loc), name(std::move(shaderName)) {}
};

} // namespace neuron

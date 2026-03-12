#pragma once
#include "neuronc/diagnostics/DiagnosticFormat.h"
#include "neuronc/parser/AST.h"
#include "neuronc/lexer/Token.h"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace neuron {

struct ParsedCallArguments {
    std::vector<ASTNodePtr> values;
    std::vector<std::string> labels;
};

struct ParserDiagnostic {
    std::string code;
    diagnostics::DiagnosticArguments arguments;
    SourceLocation location;
    std::string detail;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens, std::string filename = "<input>");

    /// Parse the entire program and return the root AST node.
    std::unique_ptr<ProgramNode> parse();

    /// Get accumulated error messages.
    const std::vector<std::string>& errors() const { return m_errors; }
    const std::vector<ParserDiagnostic> &diagnostics() const {
      return m_diagnostics;
    }

private:
    // Token navigation
    const Token& current() const;
    const Token& peek() const;
    const Token& lookahead(std::size_t offset) const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& message);
    bool isAtEnd() const;

    // Declarations
    ASTNodePtr parseDeclaration();
    ASTNodePtr parseModuleDecl();
    ASTNodePtr parseExpandModuleDecl();
    ASTNodePtr parseModuleCppDecl();
    ASTNodePtr parseBindingOrMethodOrClass();
    ASTNodePtr parseConstBinding();
    ASTNodePtr parseAtomicBinding();
    ASTNodePtr parseMacroDecl();
    ASTNodePtr parseMethodDecl(const std::string& name, AccessModifier access, SourceLocation loc);
    ASTNodePtr parseMethodShorthand(const std::string& name, SourceLocation loc);
    ASTNodePtr parseClassDecl(const std::string& name, AccessModifier access, SourceLocation loc);
    ASTNodePtr parseEnumDecl(const std::string& name, AccessModifier access, SourceLocation loc);
    ASTNodePtr parseShaderDecl(const std::string& name, AccessModifier access, SourceLocation loc);
    ASTNodePtr parseExternDecl();

    // Statements
    ASTNodePtr parseStatement();
    ASTNodePtr parseBlock();
    ASTNodePtr parseBlockOrWhitelistedSingleStmt(std::string_view ownerDescription);
    ASTNodePtr parseIfStmt();
    ASTNodePtr parseMatchStmt();
    ASTNodePtr parseMatchExpr();
    ASTNodePtr parseWhileStmt();
    ASTNodePtr parseForStmt();
    ASTNodePtr parseReturnStmt();
    ASTNodePtr parseTryStmt();
    ASTNodePtr parseThrowStmt();
    ASTNodePtr parseStaticAssertStmt();
    ASTNodePtr parseBreakStmt();
    ASTNodePtr parseContinueStmt();
    ASTNodePtr parseCastStmt(ASTNodePtr target, SourceLocation loc);
    ASTNodePtr parseUnsafeBlock();
    ASTNodePtr parseGpuBlock();
    ASTNodePtr parseCanvasStmt();
    ASTNodePtr parseShaderPassStmt();

    // Expressions
    ASTNodePtr parseExpression();
    ASTNodePtr parseOr();
    ASTNodePtr parseAnd();
    ASTNodePtr parseEquality();
    ASTNodePtr parseComparison();
    ASTNodePtr parseAddition();
    ASTNodePtr parseMultiplication();
    ASTNodePtr parsePower();
    ASTNodePtr parseUnary();
    ASTNodePtr parsePostfix();
    ASTNodePtr parsePrimary();
    ASTNodePtr parseTypeSpec();
    CastStepNode parseCastStep(bool* outPipelineNullable,
                               bool allowPipelineNullable);

    // Helpers
    std::vector<ParameterNode> parseParameterList();
    ParsedCallArguments parseArgumentList();
    ASTNodePtr cloneAstNode(const ASTNode* node) const;
    bool isWhitelistedImplicitBlockStmt(const ASTNode* node) const;
    bool canStartImplicitSingleStatementBody() const;
    void error(const std::string& message);
    void error(const std::string &code, diagnostics::DiagnosticArguments arguments,
               const std::string &detail);
    void synchronize();
    void recoverNoProgress();
    ASTNodePtr takePendingDeclaration();
    void queuePendingDeclaration(ASTNodePtr node);

    std::vector<Token> m_tokens;
    std::string m_filename;
    size_t m_pos = 0;
    int m_methodDepth = 0;
    std::vector<std::string> m_errors;
    std::vector<ParserDiagnostic> m_diagnostics;
    std::vector<ASTNodePtr> m_pendingDeclarations;
};

} // namespace neuron

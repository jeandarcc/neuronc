#include "neuronc/parser/Parser.h"

namespace neuron {

std::unique_ptr<ProgramNode> Parser::parse() {
  auto program =
      std::make_unique<ProgramNode>(SourceLocation{1, 1, m_filename});

  size_t slashPos = m_filename.find_last_of("/\\");
  std::string basename = (slashPos != std::string::npos)
                             ? m_filename.substr(slashPos + 1)
                             : m_filename;
  size_t extPos = basename.find_last_of('.');
  program->moduleName =
      (extPos != std::string::npos) ? basename.substr(0, extPos) : basename;

  while (!isAtEnd() || !m_pendingDeclarations.empty()) {
    try {
      const size_t beforePos = m_pos;
      const size_t beforePending = m_pendingDeclarations.size();
      auto decl = parseDeclaration();
      if (decl) {
        program->declarations.push_back(std::move(decl));
      } else if (m_pos == beforePos &&
          m_pendingDeclarations.size() == beforePending) {
        recoverNoProgress();
      }
    } catch (...) {
      synchronize();
    }
  }

  return program;
}

ASTNodePtr Parser::parseDeclaration() {
  if (ASTNodePtr pending = takePendingDeclaration()) {
    return pending;
  }
  if (check(TokenType::Expand)) {
    return parseExpandModuleDecl();
  }
  if (check(TokenType::Module)) {
    return parseModuleDecl();
  }
  if (check(TokenType::ModuleCpp)) {
    error(
        "The 'modulecpp' declaration was removed. Use standard modules and "
        "extern declarations instead.");
    synchronize();
    return nullptr;
  }
  if (check(TokenType::Macro)) {
    return parseMacroDecl();
  }
  if (check(TokenType::Extern)) {
    return parseExternDecl();
  }
  return parseStatement();
}

ASTNodePtr Parser::parseModuleDecl() {
  auto loc = current().location;
  expect(TokenType::Module, "Expected 'module'");
  auto name = expect(TokenType::Identifier, "Expected module name");
  expect(TokenType::Semicolon, "Expected ';' after module declaration");
  return std::make_unique<ModuleDeclNode>(name.value, loc);
}

ASTNodePtr Parser::parseExpandModuleDecl() {
  auto loc = current().location;
  expect(TokenType::Expand, "Expected 'expand'");
  expect(TokenType::Module, "Expected 'module' after 'expand'");
  auto name = expect(TokenType::Identifier, "Expected module name");
  expect(TokenType::Semicolon, "Expected ';' after expand module declaration");
  return std::make_unique<ExpandModuleDeclNode>(name.value, loc);
}

ASTNodePtr Parser::parseModuleCppDecl() {
  auto loc = current().location;
  expect(TokenType::ModuleCpp, "Expected 'modulecpp'");
  auto name = expect(TokenType::Identifier, "Expected modulecpp name");
  expect(TokenType::Semicolon, "Expected ';' after modulecpp declaration");
  return std::make_unique<ModuleCppDeclNode>(name.value, loc);
}

ASTNodePtr Parser::parseMacroDecl() {
  auto loc = current().location;
  expect(TokenType::Macro, "Expected 'macro'");
  auto macroName = expect(TokenType::Identifier, "Expected macro name");
  expect(TokenType::Semicolon, "Expected ';' after macro declaration");
  return std::make_unique<MacroDeclNode>(macroName.value, loc);
}

} // namespace neuron


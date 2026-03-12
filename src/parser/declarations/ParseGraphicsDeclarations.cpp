#include "neuronc/parser/Parser.h"

#include <cctype>

namespace neuron {

namespace {

std::string toLowerAscii(std::string value) {
  for (char &ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

CanvasEventKind canvasEventFromName(const std::string &rawName) {
  const std::string name = toLowerAscii(rawName);
  if (name == "onopen") {
    return CanvasEventKind::OnOpen;
  }
  if (name == "onframe") {
    return CanvasEventKind::OnFrame;
  }
  if (name == "onresize") {
    return CanvasEventKind::OnResize;
  }
  if (name == "onclose") {
    return CanvasEventKind::OnClose;
  }
  return CanvasEventKind::Unknown;
}

std::string canvasEventCanonicalName(CanvasEventKind kind) {
  switch (kind) {
  case CanvasEventKind::OnOpen:
    return "OnOpen";
  case CanvasEventKind::OnFrame:
    return "OnFrame";
  case CanvasEventKind::OnResize:
    return "OnResize";
  case CanvasEventKind::OnClose:
    return "OnClose";
  default:
    return "";
  }
}

} // namespace

ASTNodePtr Parser::parseShaderDecl(const std::string &name,
                                   AccessModifier access, SourceLocation loc) {
  expect(TokenType::Shader, "Expected 'shader'");
  auto shader = std::make_unique<ShaderDeclNode>(name, loc);
  shader->access = access;

  expect(TokenType::LeftBrace, "Expected '{' to open shader body");
  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    const size_t beforePos = m_pos;
    if (match(TokenType::Semicolon)) {
      continue;
    }

    if (!check(TokenType::Identifier)) {
      error("Expected shader member declaration");
      synchronize();
      continue;
    }

    Token memberNameToken = advance();
    const std::string memberName = memberNameToken.value;

    if ((memberName == "Vertex" || memberName == "Fragment") &&
        (check(TokenType::Method) ||
         (check(TokenType::Is) && lookahead(1).type == TokenType::Method))) {
      if (match(TokenType::Is)) {
      }

      ShaderStageKind stageKind = memberName == "Vertex"
                                      ? ShaderStageKind::Vertex
                                      : ShaderStageKind::Fragment;
      auto stage =
          std::make_unique<ShaderStageNode>(stageKind, memberNameToken.location);
      stage->methodDecl =
          parseMethodDecl(memberName, AccessModifier::None, memberNameToken.location);
      shader->stages.push_back(std::move(stage));
      if (m_pos == beforePos) {
        recoverNoProgress();
      }
      continue;
    }

    if (check(TokenType::Method) ||
        (check(TokenType::Is) && lookahead(1).type == TokenType::Method)) {
      if (match(TokenType::Is)) {
      }
      auto methodDecl = parseMethodDecl(memberName, AccessModifier::None,
                                        memberNameToken.location);
      shader->methods.push_back(std::move(methodDecl));
      if (m_pos == beforePos) {
        recoverNoProgress();
      }
      continue;
    }

    ASTNodePtr valueExpr = nullptr;
    ASTNodePtr typeAnnotation = nullptr;
    BindingKind kind = BindingKind::Value;

    if (match(TokenType::Is)) {
      if (check(TokenType::Another)) {
        kind = BindingKind::Copy;
        advance();
      } else if (check(TokenType::AddressOf)) {
        kind = BindingKind::AddressOf;
        advance();
      } else if (check(TokenType::ValueOf)) {
        kind = BindingKind::ValueOf;
        advance();
      } else if (check(TokenType::Move)) {
        kind = BindingKind::MoveFrom;
        advance();
      }
      valueExpr = parseExpression();
      if (match(TokenType::As)) {
        typeAnnotation = parseTypeSpec();
      }
    } else if (match(TokenType::As)) {
      typeAnnotation = parseTypeSpec();
    } else {
      error("Expected 'as' or 'is' in shader field declaration");
      synchronize();
      continue;
    }

    expect(TokenType::Semicolon, "Expected ';' after shader field declaration");
    auto uniform = std::make_unique<BindingDeclNode>(
        memberName, kind, std::move(valueExpr), memberNameToken.location);
    uniform->typeAnnotation = std::move(typeAnnotation);
    shader->uniforms.push_back(std::move(uniform));
    if (m_pos == beforePos) {
      recoverNoProgress();
    }
  }

  expect(TokenType::RightBrace, "Expected '}' to close shader body");
  match(TokenType::Semicolon);
  return shader;
}

} // namespace neuron


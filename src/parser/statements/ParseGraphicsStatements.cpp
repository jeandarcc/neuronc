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
  if (name == "onopen")
    return CanvasEventKind::OnOpen;
  if (name == "onframe")
    return CanvasEventKind::OnFrame;
  if (name == "onresize")
    return CanvasEventKind::OnResize;
  if (name == "onclose")
    return CanvasEventKind::OnClose;
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

ASTNodePtr Parser::parseGpuBlock() {
  auto loc = current().location;
  expect(TokenType::Gpu, "Expected 'gpu'");
  if (m_methodDepth == 0) {
    error("gpu block is only allowed inside method bodies");
  }

  auto node = std::make_unique<GpuBlockNode>(loc);
  if (match(TokenType::LeftParen)) {
    auto parseSelectorValue = [&]() -> std::string {
      Token valueToken = expect(TokenType::Identifier,
                                "Expected selector value in gpu(...)");
      std::string rawValue = valueToken.value;
      if (match(TokenType::Dot)) {
        Token enumMember = expect(TokenType::Identifier,
                                  "Expected enum member after '.' in gpu selector");
        rawValue += ".";
        rawValue += enumMember.value;
      }
      std::size_t dotPos = rawValue.find_last_of('.');
      if (dotPos != std::string::npos && dotPos + 1 < rawValue.size()) {
        rawValue = rawValue.substr(dotPos + 1);
      }
      return toLowerAscii(rawValue);
    };

    do {
      Token keyToken =
          expect(TokenType::Identifier, "Expected selector key in gpu(...)");
      std::string key = toLowerAscii(keyToken.value);
      expect(TokenType::Colon, "Expected ':' in gpu selector");
      std::string value = parseSelectorValue();

      if (key == "prefer" || key == "force") {
        node->preferenceMode = (key == "prefer")
                                   ? GpuDevicePreferenceMode::Prefer
                                   : GpuDevicePreferenceMode::Force;
        if (value == "discrete") {
          node->preferenceTarget = GpuDevicePreferenceTarget::Discrete;
        } else if (value == "integrated") {
          node->preferenceTarget = GpuDevicePreferenceTarget::Integrated;
        } else {
          error("Expected 'discrete' or 'integrated' in gpu selector");
        }
      } else if (key == "policy") {
        if (value == "prefer") {
          node->preferenceMode = GpuDevicePreferenceMode::Prefer;
        } else if (value == "force") {
          node->preferenceMode = GpuDevicePreferenceMode::Force;
        } else {
          error("Expected GPUPolicy.Prefer or GPUPolicy.Force in gpu selector");
        }
      } else if (key == "mode" || key == "target") {
        if (value == "discrete") {
          node->preferenceTarget = GpuDevicePreferenceTarget::Discrete;
        } else if (value == "integrated") {
          node->preferenceTarget = GpuDevicePreferenceTarget::Integrated;
        } else {
          error("Expected GPUMode.Discrete or GPUMode.Integrated in gpu selector");
        }
      } else {
        error("Unknown gpu selector key: '" + keyToken.value + "'");
      }
    } while (match(TokenType::Comma));

    expect(TokenType::RightParen, "Expected ')' after gpu(...) selector");
  }
  node->body = parseBlock();
  return node;
}

ASTNodePtr Parser::parseCanvasStmt() {
  auto loc = current().location;
  expect(TokenType::Canvas, "Expected 'canvas'");
  if (m_methodDepth == 0) {
    error("canvas block is only allowed inside method bodies");
  }

  auto canvas = std::make_unique<CanvasStmtNode>(loc);
  expect(TokenType::LeftParen, "Expected '(' after 'canvas'");
  canvas->windowExpr = parseExpression();

  while (match(TokenType::Comma)) {
    Token keyToken =
        expect(TokenType::Identifier, "Expected event key in canvas(...)");
    expect(TokenType::Colon, "Expected ':' in canvas(...) event binding");
    Token methodToken = expect(TokenType::Identifier,
                               "Expected method name in canvas event binding");
    CanvasEventKind kind = canvasEventFromName(keyToken.value);
    auto handler = std::make_unique<CanvasEventHandlerNode>(
        kind, canvasEventCanonicalName(kind), keyToken.location);
    handler->isExternalBinding = true;
    handler->externalMethodName = methodToken.value;
    if (kind == CanvasEventKind::Unknown) {
      error("Unknown canvas event key: '" + keyToken.value + "'");
    }
    canvas->handlers.push_back(std::move(handler));
  }
  expect(TokenType::RightParen, "Expected ')' after canvas(...)");

  expect(TokenType::LeftBrace, "Expected '{' to open canvas block");
  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    const size_t beforePos = m_pos;
    if (match(TokenType::Semicolon)) {
      continue;
    }

    if (!check(TokenType::Identifier)) {
      error("Expected canvas event handler name");
      synchronize();
      continue;
    }

    Token eventToken = advance();
    CanvasEventKind kind = canvasEventFromName(eventToken.value);
    if (kind == CanvasEventKind::Unknown) {
      error("Unknown canvas event handler: '" + eventToken.value + "'");
      synchronize();
      continue;
    }

    ASTNodePtr eventMethod = nullptr;
    if (check(TokenType::LeftBrace)) {
      eventMethod =
          parseMethodShorthand(canvasEventCanonicalName(kind), eventToken.location);
    } else if (match(TokenType::Is)) {
      if (!check(TokenType::Method)) {
        error("Expected 'method' after 'is' in canvas event handler");
        synchronize();
        continue;
      }
      eventMethod = parseMethodDecl(canvasEventCanonicalName(kind),
                                    AccessModifier::None, eventToken.location);
    } else if (check(TokenType::Method)) {
      eventMethod = parseMethodDecl(canvasEventCanonicalName(kind),
                                    AccessModifier::None, eventToken.location);
    } else {
      error("Expected '{' or method declaration in canvas event handler");
      synchronize();
      continue;
    }

    auto handler = std::make_unique<CanvasEventHandlerNode>(
        kind, canvasEventCanonicalName(kind), eventToken.location);
    handler->handlerMethod = std::move(eventMethod);
    canvas->handlers.push_back(std::move(handler));
    if (m_pos == beforePos) {
      recoverNoProgress();
    }
  }

  expect(TokenType::RightBrace, "Expected '}' to close canvas block");
  match(TokenType::Semicolon);
  return canvas;
}

ASTNodePtr Parser::parseShaderPassStmt() {
  auto loc = current().location;
  expect(TokenType::Pass, "Expected 'pass'");
  Token varying =
      expect(TokenType::Identifier, "Expected varying name after 'pass'");
  expect(TokenType::Semicolon, "Expected ';' after pass statement");
  return std::make_unique<ShaderPassStmtNode>(varying.value, loc);
}

} // namespace neuron


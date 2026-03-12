#include "LspTransport.h"

#include "lsp/LspPath.h"

#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <string_view>

#if defined(_WIN32)
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#endif

namespace neuron::lsp {

LspTransport::LspTransport(std::istream &input, std::ostream &output,
                           bool manageStdio)
    : m_input(input), m_output(output) {
  if (manageStdio) {
    configureBinaryMode(input, output);
  }
}

ReadMessageResult LspTransport::readMessage() {
  const std::optional<std::string> payload = readPayload();
  if (!payload.has_value()) {
    ReadMessageResult result;
    result.eof = true;
    return result;
  }

  auto parsed = llvm::json::parse(*payload);
  if (!parsed) {
    return ReadMessageResult{};
  }
  ReadMessageResult result;
  result.message = std::move(*parsed);
  return result;
}

void LspTransport::sendNotification(const std::string &method,
                                    llvm::json::Value params) {
  llvm::json::Object message;
  message["jsonrpc"] = "2.0";
  message["method"] = method;
  message["params"] = std::move(params);
  writeMessage(llvm::json::Value(std::move(message)));
}

void LspTransport::sendResult(const llvm::json::Value &id,
                              llvm::json::Value result) {
  llvm::json::Object message;
  message["jsonrpc"] = "2.0";
  message["id"] = id;
  message["result"] = std::move(result);
  writeMessage(llvm::json::Value(std::move(message)));
}

void LspTransport::sendError(const llvm::json::Value &id, int code,
                             const std::string &messageText) {
  llvm::json::Object message;
  message["jsonrpc"] = "2.0";
  message["id"] = id;
  message["error"] =
      llvm::json::Object{{"code", code}, {"message", messageText}};
  writeMessage(llvm::json::Value(std::move(message)));
}

void LspTransport::configureBinaryMode(std::istream &input, std::ostream &output) {
#if defined(_WIN32)
  if (&input == &std::cin) {
    _setmode(_fileno(stdin), _O_BINARY);
  }
  if (&output == &std::cout) {
    _setmode(_fileno(stdout), _O_BINARY);
  }
#else
  (void)input;
  (void)output;
#endif
}

std::optional<std::string> LspTransport::readPayload() {
  std::string line;
  int contentLength = -1;

  while (std::getline(m_input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    constexpr std::string_view header = "Content-Length:";
    if (line.rfind(header, 0) == 0) {
      contentLength = std::stoi(detail::trimCopy(line.substr(header.size())));
    }
  }

  if (contentLength < 0) {
    return std::nullopt;
  }

  std::string body(static_cast<std::size_t>(contentLength), '\0');
  m_input.read(body.data(), contentLength);
  if (!m_input) {
    return std::nullopt;
  }
  return body;
}

void LspTransport::writeMessage(const llvm::json::Value &message) {
  std::string payload;
  llvm::raw_string_ostream stream(payload);
  stream << message;
  stream.flush();
  m_output << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
  m_output.flush();
}

} // namespace neuron::lsp

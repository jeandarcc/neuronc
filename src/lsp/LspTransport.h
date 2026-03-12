#pragma once

#include "llvm/Support/JSON.h"

#include <iostream>
#include <iosfwd>
#include <optional>
#include <string>

namespace neuron::lsp {

struct ReadMessageResult {
  bool eof = false;
  std::optional<llvm::json::Value> message;
};

class LspTransport {
public:
  explicit LspTransport(std::istream &input = std::cin,
                        std::ostream &output = std::cout,
                        bool manageStdio = true);

  ReadMessageResult readMessage();
  void sendNotification(const std::string &method, llvm::json::Value params);
  void sendResult(const llvm::json::Value &id, llvm::json::Value result);
  void sendError(const llvm::json::Value &id, int code,
                 const std::string &messageText);

private:
  std::istream &m_input;
  std::ostream &m_output;

  static void configureBinaryMode(std::istream &input, std::ostream &output);
  std::optional<std::string> readPayload();
  void writeMessage(const llvm::json::Value &message);
};

} // namespace neuron::lsp

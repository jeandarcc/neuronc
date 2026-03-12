#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

#include <sstream>

TEST(LspTransportReadsAndWritesJsonRpcMessages) {
  const std::string payload = R"({"jsonrpc":"2.0","id":1,"method":"ping"})";
  std::stringstream input;
  input << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
  std::ostringstream output;

  neuron::lsp::LspTransport transport(input, output, false);
  const neuron::lsp::ReadMessageResult result = transport.readMessage();
  ASSERT_FALSE(result.eof);
  ASSERT_TRUE(result.message.has_value());
  const auto *message = result.message->getAsObject();
  ASSERT_TRUE(message != nullptr);
  ASSERT_EQ(message->getString("method").value_or("").str(), "ping");

  transport.sendResult(llvm::json::Value(1),
                       llvm::json::Object{{"ok", true}});
  const std::string written = output.str();
  ASSERT_TRUE(written.find("Content-Length: ") == 0);
  ASSERT_TRUE(written.find("\"ok\":true") != std::string::npos);
  return true;
}

TEST(LspDispatcherRoutesHandlersAndErrorsUnknownRequests) {
  std::stringstream input;
  std::ostringstream output;
  neuron::lsp::LspTransport transport(input, output, false);
  neuron::lsp::LspDispatcher dispatcher(transport);

  bool handled = false;
  dispatcher.registerHandler("known",
                             [&](const neuron::lsp::LspMessageContext &context)
                                 -> neuron::lsp::DispatchResult {
                               handled = context.id != nullptr;
                               return neuron::lsp::DispatchResult::Continue;
                             });

  llvm::json::Object known;
  known["jsonrpc"] = "2.0";
  known["id"] = 1;
  known["method"] = "known";
  ASSERT_EQ(dispatcher.dispatch(known), neuron::lsp::DispatchResult::Continue);
  ASSERT_TRUE(handled);

  llvm::json::Object unknown;
  unknown["jsonrpc"] = "2.0";
  unknown["id"] = 2;
  unknown["method"] = "missing";
  ASSERT_EQ(dispatcher.dispatch(unknown), neuron::lsp::DispatchResult::Continue);
  ASSERT_TRUE(output.str().find("\"code\":-32601") != std::string::npos);
  return true;
}

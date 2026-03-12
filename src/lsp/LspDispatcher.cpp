#include "LspDispatcher.h"

namespace neuron::lsp {

LspDispatcher::LspDispatcher(LspTransport &transport) : m_transport(transport) {}

void LspDispatcher::registerHandler(std::string method, Handler handler) {
  m_handlers[std::move(method)] = std::move(handler);
}

DispatchResult LspDispatcher::dispatch(const llvm::json::Object &message) const {
  const std::optional<llvm::StringRef> methodValue = message.getString("method");
  if (!methodValue.has_value()) {
    return DispatchResult::Continue;
  }

  const std::string method(methodValue->str());
  const llvm::json::Value *id = message.get("id");
  const llvm::json::Object *params = message.getObject("params");

  auto it = m_handlers.find(method);
  if (it == m_handlers.end()) {
    if (id != nullptr) {
      m_transport.sendError(*id, -32601, "Method not found");
    }
    return DispatchResult::Continue;
  }

  return it->second(LspMessageContext{method, message, id, params});
}

} // namespace neuron::lsp

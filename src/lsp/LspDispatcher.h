#pragma once

#include "LspTypes.h"
#include "lsp/LspTransport.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace neuron::lsp {

struct LspMessageContext {
  std::string method;
  const llvm::json::Object &message;
  const llvm::json::Value *id = nullptr;
  const llvm::json::Object *params = nullptr;
};

class LspDispatcher {
public:
  using Handler = std::function<DispatchResult(const LspMessageContext &)>;

  explicit LspDispatcher(LspTransport &transport);

  void registerHandler(std::string method, Handler handler);
  DispatchResult dispatch(const llvm::json::Object &message) const;

private:
  LspTransport &m_transport;
  std::unordered_map<std::string, Handler> m_handlers;
};

} // namespace neuron::lsp

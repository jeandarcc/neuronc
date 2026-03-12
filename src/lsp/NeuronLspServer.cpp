#include "NeuronLspServer.h"

#include "CallHierarchyHandler.h"
#include "CodeActionHandler.h"
#include "CompletionHandler.h"
#include "DebugViewHandler.h"
#include "DefinitionHandler.h"
#include "DiagnosticsHandler.h"
#include "DocumentManager.h"
#include "HoverHandler.h"
#include "RenameHandler.h"
#include "SymbolsHandler.h"
#include "TypeHierarchyHandler.h"
#include "WorkspaceSymbolsHandler.h"

#include "lsp/LspDispatcher.h"
#include "lsp/LspFeaturePayloads.h"
#include "lsp/LspPath.h"
#include "lsp/LspTransport.h"
#include "main/UserProfileSettings.h"
#include "neuronc/diagnostics/DiagnosticLocale.h"

namespace neuron::lsp {

namespace fs = std::filesystem;

NeuronLspServer::NeuronLspServer(fs::path toolRoot)
    : m_toolRoot(std::move(toolRoot)) {
  const auto settings = neuron::loadUserProfileSettings();
  m_userLanguagePreference =
      settings.has_value() ? settings->language : std::string("auto");
}

int NeuronLspServer::run() {
  bool shutdownRequested = false;
  LspTransport transport;
  DocumentManager documents;
  LspDispatcher dispatcher(transport);
  const std::vector<std::string> supportedLocales =
      neuron::diagnostics::loadSupportedDiagnosticLocales(m_toolRoot);
  documents.setToolRoot(m_toolRoot);
  documents.setDiagnosticLanguage(neuron::diagnostics::resolveLanguagePreference(
      m_userLanguagePreference, supportedLocales));

  DiagnosticsHandler diagnostics(dispatcher, documents, transport);
  HoverHandler hover(dispatcher, documents, transport);
  CompletionHandler completion(dispatcher, documents, transport);
  DefinitionHandler definition(dispatcher, documents, transport);
  RenameHandler rename(dispatcher, documents, transport);
  CallHierarchyHandler callHierarchy(dispatcher, documents, transport);
  TypeHierarchyHandler typeHierarchy(dispatcher, documents, transport);
  SymbolsHandler symbols(dispatcher, documents, transport);
  WorkspaceSymbolsHandler workspaceSymbols(dispatcher, documents, transport);
  CodeActionHandler codeActions(dispatcher, documents, transport);
  DebugViewHandler debugViews(dispatcher, documents, transport);

  dispatcher.registerHandler("initialize", [&](const LspMessageContext &context) {
    if (context.params != nullptr) {
      if (std::optional<llvm::StringRef> rootUri =
              context.params->getString("rootUri")) {
        documents.setWorkspaceRoot(detail::uriToPath(std::string(rootUri->str())));
      } else if (std::optional<llvm::StringRef> rootPath =
                     context.params->getString("rootPath")) {
        documents.setWorkspaceRoot(fs::path(rootPath->str()));
      }

      std::optional<std::string> clientLocale;
      if (std::optional<llvm::StringRef> locale =
              context.params->getString("locale")) {
        clientLocale = locale->str();
      }
      const std::string requestedLanguage =
          neuron::diagnostics::isLanguageAutoValue(m_userLanguagePreference)
              ? clientLocale.value_or(m_userLanguagePreference)
              : m_userLanguagePreference;
      documents.setDiagnosticLanguage(
          neuron::diagnostics::resolveLanguagePreference(
              requestedLanguage, supportedLocales,
              clientLocale.has_value() ? clientLocale : std::nullopt));
    }

    if (context.id != nullptr) {
      llvm::json::Object result;
      result["capabilities"] = detail::makeServerCapabilities();
      result["serverInfo"] =
          llvm::json::Object{{"name", "neuron-lsp"}, {"version", "0.1.0"}};
      transport.sendResult(*context.id, std::move(result));
    }
    return DispatchResult::Continue;
  });

  dispatcher.registerHandler("initialized", [](const LspMessageContext &) {
    return DispatchResult::Continue;
  });

  dispatcher.registerHandler("shutdown", [&](const LspMessageContext &context) {
    shutdownRequested = true;
    if (context.id != nullptr) {
      transport.sendResult(*context.id, llvm::json::Value(nullptr));
    }
    return DispatchResult::Continue;
  });

  dispatcher.registerHandler("exit", [](const LspMessageContext &) {
    return DispatchResult::Stop;
  });

  while (true) {
    ReadMessageResult readResult = transport.readMessage();
    if (readResult.eof) {
      return shutdownRequested ? 0 : 1;
    }
    if (!readResult.message.has_value()) {
      continue;
    }

    const auto *object = readResult.message->getAsObject();
    if (object == nullptr) {
      continue;
    }
    if (dispatcher.dispatch(*object) == DispatchResult::Stop) {
      return shutdownRequested ? 0 : 1;
    }
  }
}

} // namespace neuron::lsp

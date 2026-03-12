#include "DocumentManager.h"

#include "lsp/LspDebugViews.h"
#include "lsp/LspDocumentQueries.h"
#include "lsp/LspPath.h"
#include "lsp/LspText.h"
#include "lsp/LspWorkspace.h"

#include "neuronc/mir/MIRBuilder.h"
#include "neuronc/mir/MIROwnershipPass.h"

#include <algorithm>

namespace neuron::lsp {

namespace {

DocumentChunk parseChunk(const std::string &fullText, const fs::path &path,
                         const ChunkSpan &span) {
  DocumentChunk chunk;
  chunk.span = span;
  chunk.text = fullText.substr(span.startOffset, span.endOffset - span.startOffset);

  std::string paddedSource;
  paddedSource.reserve(span.startOffset + chunk.text.size());
  for (std::size_t i = 0; i < span.startOffset && i < fullText.size(); ++i) {
    paddedSource.push_back(fullText[i] == '\n' ? '\n' : ' ');
  }
  paddedSource += chunk.text;
  chunk.parseResult = frontend::lexAndParseSource(paddedSource, path.string());
  return chunk;
}

bool sameChunkAnchor(const DocumentChunk &lhs, const ChunkSpan &rhs,
                     const std::string &text) {
  return lhs.text == text && lhs.span.startLine == rhs.startLine &&
         lhs.span.startColumn == rhs.startColumn;
}

void appendDiagnostics(std::vector<frontend::Diagnostic> *out,
                       const std::vector<frontend::Diagnostic> &diagnostics) {
  if (out == nullptr) {
    return;
  }
  out->insert(out->end(), diagnostics.begin(), diagnostics.end());
}

} // namespace

void DocumentManager::setWorkspaceRoot(fs::path root) {
  m_workspaceRoot = std::move(root);
}

const fs::path &DocumentManager::configuredWorkspaceRoot() const {
  return m_workspaceRoot;
}

void DocumentManager::setToolRoot(fs::path root) {
  m_toolRoot = std::move(root);
  m_localizer.setToolRoot(m_toolRoot);
}

const fs::path &DocumentManager::toolRoot() const { return m_toolRoot; }

void DocumentManager::setDiagnosticLanguage(
    neuron::diagnostics::ResolvedLanguage language) {
  m_diagnosticLanguage = std::move(language);
}

const neuron::diagnostics::ResolvedLanguage &
DocumentManager::diagnosticLanguage() const {
  return m_diagnosticLanguage;
}

DocumentState &DocumentManager::openDocument(std::string uri, std::string text,
                                             int version) {
  auto [it, inserted] = m_documents.emplace(uri, DocumentState{});
  (void)inserted;
  it->second.uri = it->first;
  it->second.path = detail::uriToPath(it->first);
  it->second.text = std::move(text);
  it->second.version = version;
  it->second.chunks.clear();
  it->second.expandedTokens.clear();
  it->second.macroExpansions.clear();
  it->second.ownershipHints.clear();
  it->second.configDiagnostics.clear();
  it->second.diagnostics.clear();
  it->second.analyzer.reset();
  it->second.declarations.clear();
  return it->second;
}

DocumentState *DocumentManager::applyChanges(
    const std::string &uri, const std::vector<DocumentChange> &changes,
    int version) {
  DocumentState *document = find(uri);
  if (document == nullptr) {
    return nullptr;
  }

  document->version = version;
  for (const DocumentChange &change : changes) {
    if (!change.range.has_value()) {
      document->text = change.text;
      continue;
    }

    const std::size_t startOffset =
        detail::positionToOffset(document->text, change.range->start.line - 1,
                                 change.range->start.column - 1);
    const std::size_t endOffset =
        detail::positionToOffset(document->text, change.range->end.line - 1,
                                 change.range->end.column - 1);
    document->text.replace(startOffset, endOffset - startOffset, change.text);
  }
  return document;
}

bool DocumentManager::closeDocument(const std::string &uri) {
  return m_documents.erase(uri) > 0;
}

DocumentState *DocumentManager::find(const std::string &uri) {
  auto it = m_documents.find(uri);
  return it == m_documents.end() ? nullptr : &it->second;
}

const DocumentState *DocumentManager::find(const std::string &uri) const {
  auto it = m_documents.find(uri);
  return it == m_documents.end() ? nullptr : &it->second;
}

fs::path DocumentManager::workspaceRootFor(const fs::path &path) const {
  return detail::findWorkspaceRoot(path, m_workspaceRoot);
}

fs::path DocumentManager::effectiveWorkspaceRoot() const {
  if (!m_workspaceRoot.empty()) {
    return m_workspaceRoot;
  }
  if (m_documents.empty()) {
    return {};
  }
  return workspaceRootFor(m_documents.begin()->second.path);
}

void DocumentManager::reanalyze(DocumentState &document) {
  const fs::path workspaceRoot = workspaceRootFor(document.path);
  const std::vector<ChunkSpan> spans = detail::splitTopLevelChunks(document.text);
  std::vector<DocumentChunk> newChunks;
  newChunks.reserve(spans.size());

  for (const ChunkSpan &span : spans) {
    const std::string chunkText =
        document.text.substr(span.startOffset, span.endOffset - span.startOffset);
    auto oldIt = std::find_if(document.chunks.begin(), document.chunks.end(),
                              [&](const DocumentChunk &chunk) {
                                return sameChunkAnchor(chunk, span, chunkText);
                              });
    if (oldIt != document.chunks.end()) {
      DocumentChunk reused = std::move(*oldIt);
      reused.span = span;
      newChunks.push_back(std::move(reused));
    } else {
      newChunks.push_back(parseChunk(document.text, document.path, span));
    }
  }

  document.chunks = std::move(newChunks);
  document.diagnostics.clear();
  document.declarations.clear();
  detail::refreshMacroExpansionState(document, m_toolRoot);

  for (const auto &chunk : document.chunks) {
    appendDiagnostics(&document.diagnostics, chunk.parseResult.diagnostics);
    if (chunk.parseResult.program == nullptr) {
      continue;
    }
    for (const auto &decl : chunk.parseResult.program->declarations) {
      document.declarations.push_back(decl.get());
    }
  }

  ProgramView programView;
  programView.location = {1, 1, document.path.string()};
  programView.moduleName = detail::fileStem(document.path);
  programView.declarations = document.declarations;

  frontend::SemanticResult semanticResult =
      frontend::analyzeProgramView(programView,
                                   detail::makeSemanticOptions(document, workspaceRoot));
  appendDiagnostics(&document.diagnostics, semanticResult.diagnostics);
  appendDiagnostics(&document.diagnostics, document.configDiagnostics);
  document.analyzer = std::move(semanticResult.analyzer);

  document.ownershipHints.clear();
  for (const auto &chunk : document.chunks) {
    if (chunk.parseResult.program == nullptr || chunk.parseResult.hasErrors()) {
      continue;
    }
    mir::MIRBuilder mirBuilder;
    auto module = mirBuilder.build(chunk.parseResult.program.get(),
                                   detail::fileStem(document.path));
    if (module == nullptr || mirBuilder.hasErrors()) {
      appendDiagnostics(&document.diagnostics, frontend::convertStringDiagnostics(
                                                 "mir", document.path.string(),
                                                 mirBuilder.errors()));
      continue;
    }

    mir::MIROwnershipPass ownershipPass;
    ownershipPass.setSourceText(document.text);
    ownershipPass.run(*module);
    appendDiagnostics(&document.diagnostics,
                      frontend::convertSemanticDiagnostics(ownershipPass.errors()));
    for (const auto &hint : ownershipPass.hints()) {
      document.ownershipHints.push_back(
          OwnershipHintRecord{hint.location, hint.length, hint.label, hint.tooltip});
    }
  }

  std::sort(document.ownershipHints.begin(), document.ownershipHints.end(),
            [](const OwnershipHintRecord &lhs, const OwnershipHintRecord &rhs) {
              if (lhs.location.line != rhs.location.line) {
                return lhs.location.line < rhs.location.line;
              }
              if (lhs.location.column != rhs.location.column) {
                return lhs.location.column < rhs.location.column;
              }
              return lhs.label < rhs.label;
            });
  appendDiagnostics(&document.diagnostics,
                    detail::collectUnusedVariableDiagnostics(document));
  if (!m_diagnosticLanguage.effective.empty()) {
    document.diagnostics = m_localizer.localizeDiagnostics(
        document.diagnostics, m_diagnosticLanguage.effective);
  }
}

std::optional<std::string>
DocumentManager::readWorkspaceFile(const fs::path &path) const {
  const fs::path normalized = detail::normalizePath(path);
  for (const auto &entry : m_documents) {
    if (detail::samePath(entry.second.path, normalized)) {
      return entry.second.text;
    }
  }
  return detail::readFileText(normalized);
}

std::vector<fs::path>
DocumentManager::collectWorkspaceFiles(const fs::path &workspaceRoot) const {
  std::unordered_map<std::string, fs::path> uniquePaths;
  const auto addPath = [&](const fs::path &path) {
    if (path.extension() != ".npp") {
      return;
    }
    const fs::path normalized = detail::normalizePath(path);
    uniquePaths[detail::pathKey(normalized)] = normalized;
  };

  for (const auto &entry : m_documents) {
    addPath(entry.second.path);
  }

  if (!workspaceRoot.empty()) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(workspaceRoot, ec), end; it != end;
         it.increment(ec)) {
      if (ec) {
        continue;
      }
      if (it->is_directory()) {
        const std::string name = it->path().filename().string();
        if (name == ".git" || name == ".vs" || name == "node_modules" ||
            name == "build" || name == "build-mingw") {
          it.disable_recursion_pending();
        }
        continue;
      }
      if (it->is_regular_file() && it->path().extension() == ".npp") {
        addPath(it->path());
      }
    }
  }

  std::vector<fs::path> files;
  files.reserve(uniquePaths.size());
  for (const auto &entry : uniquePaths) {
    files.push_back(entry.second);
  }
  std::sort(files.begin(), files.end(),
            [](const fs::path &lhs, const fs::path &rhs) {
              return lhs.generic_string() < rhs.generic_string();
            });
  return files;
}

std::optional<WorkspaceSemanticState>
DocumentManager::analyzeWorkspaceFile(const fs::path &path,
                                      const fs::path &workspaceRoot) const {
  const fs::path normalizedPath = detail::normalizePath(path);
  const std::vector<ResolvedWorkspaceSource> sources =
      detail::resolveWorkspaceSources(
          normalizedPath, workspaceRoot,
          [&](const fs::path &sourcePath) { return readWorkspaceFile(sourcePath); });
  if (sources.empty()) {
    return std::nullopt;
  }

  std::vector<std::unique_ptr<ProgramNode>> programs;
  programs.reserve(sources.size());

  WorkspaceSemanticState state;
  state.entryPath = normalizedPath;

  for (const ResolvedWorkspaceSource &source : sources) {
    frontend::ParseResult parseResult =
        frontend::lexAndParseSource(source.text, source.path.string());
    if (parseResult.program == nullptr || parseResult.hasErrors()) {
      return std::nullopt;
    }
    if (detail::samePath(source.path, normalizedPath)) {
      state.entryTokens = parseResult.tokens;
    }
    programs.push_back(std::move(parseResult.program));
  }

  state.mergedProgram =
      detail::mergeResolvedPrograms(&programs, normalizedPath.stem().string());
  state.semanticResult = frontend::analyzeProgram(
      state.mergedProgram.get(),
      detail::makeWorkspaceSemanticOptions(normalizedPath, workspaceRoot));
  if (state.semanticResult.analyzer == nullptr) {
    return std::nullopt;
  }
  return state;
}

} // namespace neuron::lsp

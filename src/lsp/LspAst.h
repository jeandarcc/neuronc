#pragma once

#include "LspTypes.h"

#include <functional>

namespace neuron::lsp::detail {

void walkAst(const ASTNode *node,
             const std::function<void(const ASTNode *)> &visitor);
SourceLocation astNodeEndLocation(const ASTNode *node);
int compareSourceLocations(const SourceLocation &lhs, const SourceLocation &rhs);
bool locationWithinSpan(const SourceLocation &location,
                        const SourceLocation &start,
                        const SourceLocation &end);
SymbolRange makePointSymbolRange(const SourceLocation &location, int length);
SymbolRange makeNodeSymbolRange(const ASTNode *node, int selectionLength);

const MethodDeclNode *findMethodByLocation(const ASTNode *root,
                                           const SourceLocation &location);
const ClassDeclNode *findClassByLocation(const ASTNode *root,
                                         const SourceLocation &location);
const BindingDeclNode *findBindingByLocation(const ASTNode *root,
                                             const SourceLocation &location);
const MethodDeclNode *findEnclosingMethod(const ASTNode *root,
                                          const SourceLocation &location);
const ClassDeclNode *findOwningClass(const ASTNode *root,
                                     const MethodDeclNode *method);
const CallExprNode *findCallByCalleeLocation(const ASTNode *root,
                                             const SourceLocation &location);

} // namespace neuron::lsp::detail

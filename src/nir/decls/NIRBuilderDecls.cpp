#include "neuronc/nir/NIRBuilder.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace neuron::nir {

namespace {

ShaderBindingKind shaderBindingKindForTypeName(const std::string &typeName) {
  if (typeName == "Color" || typeName == "Vec4" || typeName == "Vector4") {
    return ShaderBindingKind::Vec4;
  }
  if (typeName == "Texture2D") {
    return ShaderBindingKind::Texture2D;
  }
  if (typeName == "Sampler") {
    return ShaderBindingKind::Sampler;
  }
  if (typeName == "Matrix4") {
    return ShaderBindingKind::Matrix4;
  }
  return ShaderBindingKind::Unknown;
}

std::string glslTypeForShaderTypeName(const std::string &typeName) {
  if (typeName == "Color" || typeName == "Vector4" || typeName == "Vec4") {
    return "vec4";
  }
  if (typeName == "Vector3") {
    return "vec3";
  }
  if (typeName == "Vector2") {
    return "vec2";
  }
  if (typeName == "Matrix4") {
    return "mat4";
  }
  if (typeName == "float" || typeName == "double") {
    return "float";
  }
  if (typeName == "int") {
    return "int";
  }
  if (typeName == "Texture2D") {
    return "texture2D";
  }
  if (typeName == "Sampler") {
    return "sampler";
  }
  return "";
}

std::string formatShaderFloat(double value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(6) << value;
  std::string text = out.str();
  while (text.size() > 2 && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.push_back('0');
  }
  return text;
}

uint32_t alignShaderUniformOffset(uint32_t offset) {
  return (offset + 15u) & ~15u;
}

uint32_t shaderVertexLayoutBit(const std::string &name) {
  if (name == "position") {
    return ShaderVertexLayoutPosition;
  }
  if (name == "uv") {
    return ShaderVertexLayoutUv;
  }
  if (name == "normal") {
    return ShaderVertexLayoutNormal;
  }
  return 0u;
}

std::string resolveCallNameLocal(ASTNode *callee) {
  if (callee == nullptr) {
    return "";
  }
  if (callee->type == ASTNodeType::Identifier) {
    return static_cast<IdentifierNode *>(callee)->name;
  }
  if (callee->type == ASTNodeType::TypeSpec) {
    return static_cast<TypeSpecNode *>(callee)->typeName;
  }
  if (callee->type == ASTNodeType::MemberAccessExpr) {
    auto *member = static_cast<MemberAccessNode *>(callee);
    const std::string base = resolveCallNameLocal(member->object.get());
    if (base.empty()) {
      return "";
    }
    return base + "." + member->member;
  }
  return "";
}

std::string materialSetRuntimeFunction(ShaderBindingKind kind) {
  switch (kind) {
  case ShaderBindingKind::Vec4:
    return "Material.SetVec4";
  case ShaderBindingKind::Texture2D:
    return "Material.SetTexture";
  case ShaderBindingKind::Sampler:
    return "Material.SetSampler";
  case ShaderBindingKind::Matrix4:
    return "Material.SetMatrix4";
  default:
    return "";
  }
}

std::string resolveDescriptorSetFunc(const Module *module,
                                     const std::string &shaderName,
                                     const std::string &fieldName) {
  if (module == nullptr || shaderName.empty() || fieldName.empty()) {
    return "";
  }
  const ShaderDesc *shader = module->findShader(shaderName);
  if (shader == nullptr) {
    return "";
  }
  for (const auto &binding : shader->bindings) {
    if (binding.name == fieldName) {
      return materialSetRuntimeFunction(binding.kind);
    }
  }
  return "";
}

struct ShaderEmitValue {
  std::string code;
  std::string typeName;
  bool valid = true;
};

struct ShaderStageEmitState {
  NIRBuilder *builder = nullptr;
  ShaderStageKind stageKind = ShaderStageKind::Vertex;
  std::unordered_map<std::string, std::string> knownTypes;
  std::unordered_map<std::string, std::string> passedTypes;
  bool usesMvp = false;
};

std::string shaderIdentifierToGlsl(ShaderStageEmitState &state,
                                   const std::string &name) {
  auto typeIt = state.knownTypes.find(name);
  if (typeIt == state.knownTypes.end()) {
    return "";
  }
  if (name == "MVP") {
    state.usesMvp = true;
    return "shader_globals.u_MVP";
  }
  if (typeIt->second == "Color" || typeIt->second == "Vector4" ||
      typeIt->second == "Vec4" || typeIt->second == "Matrix4") {
    return "shader_globals." + name;
  }
  if (name == "position" || name == "uv" || name == "normal") {
    if (state.stageKind == ShaderStageKind::Vertex) {
      return "in_" + name;
    }
    if (state.passedTypes.find(name) != state.passedTypes.end()) {
      return "v_" + name;
    }
  }
  if (state.passedTypes.find(name) != state.passedTypes.end()) {
    return "v_" + name;
  }
  return name;
}

ShaderEmitValue emitShaderExpression(ShaderStageEmitState &state, ASTNode *expr);

ShaderEmitValue emitShaderCall(ShaderStageEmitState &state, CallExprNode *call) {
  if (call == nullptr) {
    return {"", "<error>", false};
  }

  const std::string callName = resolveCallNameLocal(call->callee.get());
  if (callName == "Color" || callName == "Vector2" || callName == "Vector3" ||
      callName == "Vector4") {
    const std::string resultType =
        callName == "Color" ? "Color" : callName;
    const std::string glslCtor =
        callName == "Color" ? "vec4" : glslTypeForShaderTypeName(callName);
    std::vector<std::string> args;
    args.reserve(call->arguments.size());
    for (const auto &arg : call->arguments) {
      ShaderEmitValue lowered = emitShaderExpression(state, arg.get());
      if (!lowered.valid) {
        return {"", "<error>", false};
      }
      args.push_back(lowered.code);
    }
    std::ostringstream out;
    out << glslCtor << "(";
    for (size_t i = 0; i < args.size(); ++i) {
      if (i != 0) {
        out << ", ";
      }
      out << args[i];
    }
    out << ")";
    return {out.str(), resultType, true};
  }

  if (call->callee != nullptr &&
      call->callee->type == ASTNodeType::MemberAccessExpr) {
    auto *member = static_cast<MemberAccessNode *>(call->callee.get());
    if (member->member == "Sample") {
      if (state.stageKind != ShaderStageKind::Fragment) {
        state.builder->reportError(call->location,
                                   "Texture2D.Sample(...) is only supported in Fragment stage.");
        return {"", "<error>", false};
      }
      if (call->arguments.size() != 2) {
        state.builder->reportError(call->location,
                                   "Texture2D.Sample(...) expects sampler and uv arguments.");
        return {"", "<error>", false};
      }
      ShaderEmitValue textureValue =
          emitShaderExpression(state, member->object.get());
      ShaderEmitValue samplerValue =
          emitShaderExpression(state, call->arguments[0].get());
      ShaderEmitValue uvValue =
          emitShaderExpression(state, call->arguments[1].get());
      if (!textureValue.valid || !samplerValue.valid || !uvValue.valid) {
        return {"", "<error>", false};
      }
      if (textureValue.typeName != "Texture2D" || samplerValue.typeName != "Sampler" ||
          uvValue.typeName != "Vector2") {
        state.builder->reportError(call->location,
                                   "Texture2D.Sample(...) requires Texture2D, Sampler, Vector2.");
        return {"", "<error>", false};
      }
      return {"texture(sampler2D(" + textureValue.code + ", " + samplerValue.code +
                  "), " + uvValue.code + ")",
              "Color", true};
    }
  }

  state.builder->reportError(call->location,
                             "Unsupported shader call expression.");
  return {"", "<error>", false};
}

ShaderEmitValue emitShaderExpression(ShaderStageEmitState &state, ASTNode *expr) {
  if (expr == nullptr) {
    return {"", "<error>", false};
  }

  switch (expr->type) {
  case ASTNodeType::IntLiteral:
    return {std::to_string(static_cast<IntLiteralNode *>(expr)->value), "int", true};
  case ASTNodeType::FloatLiteral:
    return {formatShaderFloat(static_cast<FloatLiteralNode *>(expr)->value),
            "float", true};
  case ASTNodeType::Identifier: {
    const std::string &name = static_cast<IdentifierNode *>(expr)->name;
    auto typeIt = state.knownTypes.find(name);
    if (typeIt == state.knownTypes.end()) {
      state.builder->reportError(expr->location,
                                 "Unsupported shader identifier: " + name);
      return {"", "<error>", false};
    }
    return {shaderIdentifierToGlsl(state, name), typeIt->second, true};
  }
  case ASTNodeType::UnaryExpr: {
    auto *unary = static_cast<UnaryExprNode *>(expr);
    if (unary->op != TokenType::Minus) {
      state.builder->reportError(expr->location,
                                 "Unsupported shader unary expression.");
      return {"", "<error>", false};
    }
    ShaderEmitValue operand = emitShaderExpression(state, unary->operand.get());
    if (!operand.valid) {
      return operand;
    }
    return {"(-" + operand.code + ")", operand.typeName, true};
  }
  case ASTNodeType::BinaryExpr: {
    auto *binary = static_cast<BinaryExprNode *>(expr);
    ShaderEmitValue lhs = emitShaderExpression(state, binary->left.get());
    ShaderEmitValue rhs = emitShaderExpression(state, binary->right.get());
    if (!lhs.valid || !rhs.valid) {
      return {"", "<error>", false};
    }

    if (binary->op == TokenType::Caret) {
      return {"pow(" + lhs.code + ", " + rhs.code + ")", lhs.typeName, true};
    }
    if (binary->op == TokenType::CaretCaret) {
      if (binary->right && binary->right->type == ASTNodeType::IntLiteral &&
          static_cast<IntLiteralNode *>(binary->right.get())->value == 2) {
        return {"sqrt(" + lhs.code + ")", lhs.typeName, true};
      }
      return {"pow(" + lhs.code + ", 1.0 / float(" + rhs.code + "))",
              lhs.typeName, true};
    }

    const char *glslOp = nullptr;
    switch (binary->op) {
    case TokenType::Plus:
      glslOp = "+";
      break;
    case TokenType::Minus:
      glslOp = "-";
      break;
    case TokenType::Star:
      glslOp = "*";
      break;
    case TokenType::Slash:
      glslOp = "/";
      break;
    default:
      state.builder->reportError(expr->location,
                                 "Unsupported shader binary operator.");
      return {"", "<error>", false};
    }

    if (binary->op == TokenType::Star && lhs.typeName == "Matrix4" &&
        rhs.typeName == "Vector3") {
      state.usesMvp = state.usesMvp || lhs.code.find("u_MVP") != std::string::npos;
      return {"(" + lhs.code + " * vec4(" + rhs.code + ", 1.0))", "Vector4", true};
    }
    if (binary->op == TokenType::Star && lhs.typeName == "Matrix4" &&
        rhs.typeName == "Vector4") {
      state.usesMvp = state.usesMvp || lhs.code.find("u_MVP") != std::string::npos;
      return {"(" + lhs.code + " * " + rhs.code + ")", "Vector4", true};
    }

    std::string resultType = lhs.typeName;
    if (lhs.typeName != rhs.typeName) {
      if ((lhs.typeName == "Color" || lhs.typeName == "Vector4" ||
           lhs.typeName == "Vector3" || lhs.typeName == "Vector2") &&
          (rhs.typeName == "float" || rhs.typeName == "int")) {
        resultType = lhs.typeName;
      } else if ((rhs.typeName == "Color" || rhs.typeName == "Vector4" ||
                  rhs.typeName == "Vector3" || rhs.typeName == "Vector2") &&
                 (lhs.typeName == "float" || lhs.typeName == "int")) {
        resultType = rhs.typeName;
      } else {
        state.builder->reportError(expr->location,
                                   "Unsupported shader operand type combination.");
        return {"", "<error>", false};
      }
    }
    return {"(" + lhs.code + " " + glslOp + " " + rhs.code + ")", resultType,
            true};
  }
  case ASTNodeType::CallExpr:
    return emitShaderCall(state, static_cast<CallExprNode *>(expr));
  default:
    state.builder->reportError(expr->location,
                               "Unsupported shader expression in lowering.");
    return {"", "<error>", false};
  }
}

std::string emitShaderUniformBlock(const ShaderDesc &shaderDesc) {
  if (shaderDesc.uniformBufferSize == 0) {
    return "";
  }

  std::ostringstream out;
  out << "layout(set = 0, binding = 0, std140) uniform ShaderGlobals {\n";
  if (shaderDesc.mvpOffset != UINT32_MAX) {
    out << "  mat4 u_MVP;\n";
  }
  for (const auto &binding : shaderDesc.bindings) {
    if (binding.kind != ShaderBindingKind::Vec4 &&
        binding.kind != ShaderBindingKind::Matrix4) {
      continue;
    }
    out << "  " << glslTypeForShaderTypeName(binding.typeName) << " "
        << binding.name << ";\n";
  }
  out << "} shader_globals;\n";
  return out.str();
}

} // namespace

void NIRBuilder::visitProgram(ProgramNode *node) {
  for (auto &decl : node->declarations) {
    if (decl->type == ASTNodeType::EnumDecl) {
      visitEnumDecl(static_cast<EnumDeclNode *>(decl.get()));
    }
  }

  for (auto &decl : node->declarations) {
    if (decl->type == ASTNodeType::MethodDecl) {
      visitMethodDecl(static_cast<MethodDeclNode *>(decl.get()));
    } else if (decl->type == ASTNodeType::ShaderDecl) {
      visitShaderDecl(static_cast<ShaderDeclNode *>(decl.get()));
    } else if (decl->type == ASTNodeType::ClassDecl) {
      visitClassDecl(static_cast<ClassDeclNode *>(decl.get()));
    } else if (decl->type == ASTNodeType::BindingDecl) {
      visitBindingDecl(static_cast<BindingDeclNode *>(decl.get()));
    } else if (decl->type == ASTNodeType::CastStmt) {
      visitCastStmt(static_cast<CastStmtNode *>(decl.get()));
    }
  }
}

void NIRBuilder::visitEnumDecl(EnumDeclNode *node) {
  if (node == nullptr) {
    return;
  }

  auto &members = m_enumMembers[node->name];
  for (size_t i = 0; i < node->members.size(); ++i) {
    members[node->members[i]] = static_cast<int64_t>(i);
  }
}

void NIRBuilder::visitShaderDecl(ShaderDeclNode *node) {
  if (node == nullptr) {
    return;
  }

  ShaderDesc shaderDesc;
  shaderDesc.name = node->name;
  uint32_t bindingSlot = 0;
  bool usesMvp = false;
  MethodDeclNode *vertexMethod = nullptr;
  MethodDeclNode *fragmentMethod = nullptr;
  std::unordered_map<std::string, std::string> uniformTypes;

  for (const auto &uniformNode : node->uniforms) {
    if (uniformNode == nullptr ||
        uniformNode->type != ASTNodeType::BindingDecl) {
      continue;
    }

    auto *uniform = static_cast<BindingDeclNode *>(uniformNode.get());
    NTypePtr uniformType =
        resolveType(uniform->typeAnnotation ? uniform->typeAnnotation.get() : nullptr);
    ShaderBindingDesc binding;
    binding.name = uniform->name;
    binding.typeName = uniformType ? uniformType->toString() : "unknown";
    binding.kind = shaderBindingKindForTypeName(binding.typeName);
    binding.slot = bindingSlot++;
    shaderDesc.bindings.push_back(std::move(binding));
    uniformTypes[uniform->name] = uniformType ? uniformType->toString() : "unknown";
  }

  for (const auto &stageNode : node->stages) {
    if (stageNode == nullptr || stageNode->type != ASTNodeType::ShaderStage) {
      continue;
    }

    const auto *stage = static_cast<ShaderStageNode *>(stageNode.get());
    if (stage->stageKind == ShaderStageKind::Vertex) {
      shaderDesc.hasVertexStage = true;
      vertexMethod = stage->methodDecl != nullptr &&
                             stage->methodDecl->type == ASTNodeType::MethodDecl
                         ? static_cast<MethodDeclNode *>(stage->methodDecl.get())
                         : nullptr;
    } else if (stage->stageKind == ShaderStageKind::Fragment) {
      shaderDesc.hasFragmentStage = true;
      fragmentMethod = stage->methodDecl != nullptr &&
                               stage->methodDecl->type == ASTNodeType::MethodDecl
                           ? static_cast<MethodDeclNode *>(stage->methodDecl.get())
                           : nullptr;
    }
  }

  if (vertexMethod != nullptr) {
    ShaderStageEmitState vertexState;
    vertexState.builder = this;
    vertexState.stageKind = ShaderStageKind::Vertex;
    vertexState.knownTypes = uniformTypes;
    vertexState.knownTypes["MVP"] = "Matrix4";

    std::ostringstream vertexOut;
    vertexOut << "#version 450\n";

    for (const auto &param : vertexMethod->parameters) {
      NTypePtr paramType = resolveType(param.typeSpec.get());
      const std::string typeName = paramType ? paramType->toString() : "unknown";
      const std::string glslType = glslTypeForShaderTypeName(typeName);
      if (glslType.empty()) {
        reportError(param.location, "Unsupported vertex input type in shader.");
        continue;
      }
      const uint32_t layoutBit = shaderVertexLayoutBit(param.name);
      if (layoutBit == 0u) {
        reportError(param.location,
                    "Unsupported vertex input name in shader.");
        continue;
      }
      shaderDesc.vertexInputs.push_back({param.name, typeName});
      shaderDesc.vertexLayoutMask |= layoutBit;
      vertexState.knownTypes[param.name] = typeName;
      const uint32_t location = param.name == "position" ? 0u :
                                (param.name == "uv" ? 1u : 2u);
      vertexOut << "layout(location = " << location << ") in " << glslType
                << " in_" << param.name << ";\n";
    }

    auto *body = vertexMethod->body != nullptr &&
                         vertexMethod->body->type == ASTNodeType::Block
                     ? static_cast<BlockNode *>(vertexMethod->body.get())
                     : nullptr;
    if (body == nullptr) {
      reportError(vertexMethod->location, "Vertex shader stage requires a block body.");
    } else {
      std::vector<std::pair<std::string, std::string>> varyingOrder;
      std::unordered_set<std::string> seenVaryings;
      for (const auto &stmt : body->statements) {
        if (stmt->type == ASTNodeType::ShaderPassStmt) {
          const std::string &varyingName =
              static_cast<ShaderPassStmtNode *>(stmt.get())->varyingName;
          auto typeIt = vertexState.knownTypes.find(varyingName);
          if (typeIt == vertexState.knownTypes.end()) {
            reportError(stmt->location, "pass references an unknown varying source.");
            continue;
          }
          vertexState.passedTypes[varyingName] = typeIt->second;
          if (seenVaryings.insert(varyingName).second) {
            varyingOrder.emplace_back(varyingName, typeIt->second);
          }
        }
      }

      for (uint32_t i = 0; i < varyingOrder.size(); ++i) {
        vertexOut << "layout(location = " << i << ") out "
                  << glslTypeForShaderTypeName(varyingOrder[i].second) << " v_"
                  << varyingOrder[i].first << ";\n";
        shaderDesc.varyings.push_back(
            {varyingOrder[i].first, varyingOrder[i].second, i});
      }

      vertexOut << "void main() {\n";
      for (const auto &stmt : body->statements) {
        if (stmt->type == ASTNodeType::ShaderPassStmt) {
          const std::string &varyingName =
              static_cast<ShaderPassStmtNode *>(stmt.get())->varyingName;
          vertexOut << "  v_" << varyingName << " = "
                    << shaderIdentifierToGlsl(vertexState, varyingName) << ";\n";
          continue;
        }
        if (stmt->type != ASTNodeType::ReturnStmt) {
          reportError(stmt->location,
                      "Vertex shader lowering only supports pass and return statements.");
          continue;
        }
        auto *ret = static_cast<ReturnStmtNode *>(stmt.get());
        ShaderEmitValue value = emitShaderExpression(vertexState, ret->value.get());
        if (!value.valid) {
          continue;
        }
        if (value.typeName == "Vector3") {
          vertexOut << "  gl_Position = vec4(" << value.code << ", 1.0);\n";
        } else {
          vertexOut << "  gl_Position = " << value.code << ";\n";
        }
        vertexOut << "  return;\n";
      }
      vertexOut << "}\n";
      usesMvp = usesMvp || vertexState.usesMvp;
      shaderDesc.vertexGlsl = vertexOut.str();
    }
  }

  if (fragmentMethod != nullptr) {
    ShaderStageEmitState fragmentState;
    fragmentState.builder = this;
    fragmentState.stageKind = ShaderStageKind::Fragment;
    fragmentState.knownTypes = uniformTypes;
    fragmentState.knownTypes["MVP"] = "Matrix4";
    for (const auto &varying : shaderDesc.varyings) {
      fragmentState.knownTypes[varying.name] = varying.typeName;
      fragmentState.passedTypes[varying.name] = varying.typeName;
    }

    std::ostringstream fragmentOut;
    fragmentOut << "#version 450\n";
    for (const auto &varying : shaderDesc.varyings) {
      fragmentOut << "layout(location = " << varying.location << ") in "
                  << glslTypeForShaderTypeName(varying.typeName) << " v_"
                  << varying.name << ";\n";
    }
    fragmentOut << "layout(location = 0) out vec4 outColor;\n";

    auto *body = fragmentMethod->body != nullptr &&
                         fragmentMethod->body->type == ASTNodeType::Block
                     ? static_cast<BlockNode *>(fragmentMethod->body.get())
                     : nullptr;
    if (body == nullptr) {
      reportError(fragmentMethod->location,
                  "Fragment shader stage requires a block body.");
    } else {
      fragmentOut << "void main() {\n";
      for (const auto &stmt : body->statements) {
        if (stmt->type != ASTNodeType::ReturnStmt) {
          reportError(stmt->location,
                      "Fragment shader lowering only supports return statements.");
          continue;
        }
        auto *ret = static_cast<ReturnStmtNode *>(stmt.get());
        ShaderEmitValue value =
            emitShaderExpression(fragmentState, ret->value.get());
        if (!value.valid) {
          continue;
        }
        fragmentOut << "  outColor = " << value.code << ";\n";
        fragmentOut << "  return;\n";
      }
      fragmentOut << "}\n";
      usesMvp = usesMvp || fragmentState.usesMvp;
      shaderDesc.fragmentGlsl = fragmentOut.str();
    }
  }

  bool needsUniformBuffer = usesMvp;
  for (const auto &binding : shaderDesc.bindings) {
    if (binding.kind == ShaderBindingKind::Vec4 ||
        binding.kind == ShaderBindingKind::Matrix4) {
      needsUniformBuffer = true;
      break;
    }
  }

  uint32_t uniformOffset = 0;
  if (usesMvp) {
    shaderDesc.mvpOffset = 0;
    uniformOffset = 64u;
  }
  for (auto &binding : shaderDesc.bindings) {
    if (binding.kind == ShaderBindingKind::Vec4) {
      uniformOffset = alignShaderUniformOffset(uniformOffset);
      binding.uniformOffset = uniformOffset;
      binding.uniformSize = 16u;
      uniformOffset += 16u;
    } else if (binding.kind == ShaderBindingKind::Matrix4) {
      uniformOffset = alignShaderUniformOffset(uniformOffset);
      binding.uniformOffset = uniformOffset;
      binding.uniformSize = 64u;
      uniformOffset += 64u;
    }
  }
  shaderDesc.uniformBufferSize = needsUniformBuffer ? uniformOffset : 0u;

  uint32_t descriptorBinding = needsUniformBuffer ? 1u : 0u;
  for (auto &binding : shaderDesc.bindings) {
    if (binding.kind == ShaderBindingKind::Vec4 ||
        binding.kind == ShaderBindingKind::Matrix4) {
      binding.descriptorBinding =
          needsUniformBuffer ? 0u : UINT32_MAX;
      continue;
    }
    binding.descriptorBinding = descriptorBinding++;
  }

  const std::string uniformBlock = emitShaderUniformBlock(shaderDesc);
  if (!uniformBlock.empty()) {
    if (!shaderDesc.vertexGlsl.empty()) {
      shaderDesc.vertexGlsl.insert(shaderDesc.vertexGlsl.find('\n') + 1,
                                   uniformBlock);
    }
    if (!shaderDesc.fragmentGlsl.empty()) {
      shaderDesc.fragmentGlsl.insert(shaderDesc.fragmentGlsl.find('\n') + 1,
                                     uniformBlock);
    }
  }
  for (const auto &binding : shaderDesc.bindings) {
    if (binding.kind != ShaderBindingKind::Texture2D &&
        binding.kind != ShaderBindingKind::Sampler) {
      continue;
    }
    std::ostringstream decl;
    decl << "layout(set = 0, binding = " << binding.descriptorBinding << ") uniform "
         << glslTypeForShaderTypeName(binding.typeName) << " " << binding.name
         << ";\n";
    if (!shaderDesc.vertexGlsl.empty()) {
      shaderDesc.vertexGlsl.insert(shaderDesc.vertexGlsl.find('\n') + 1, decl.str());
    }
    if (!shaderDesc.fragmentGlsl.empty()) {
      shaderDesc.fragmentGlsl.insert(shaderDesc.fragmentGlsl.find('\n') + 1,
                                     decl.str());
    }
  }

  m_module->addShader(std::move(shaderDesc));
  for (auto &methodNode : node->methods) {
    if (methodNode == nullptr || methodNode->type != ASTNodeType::MethodDecl) {
      continue;
    }
    auto *method = static_cast<MethodDeclNode *>(methodNode.get());
    const std::string originalName = method->name;
    method->name = node->name + "." + method->name;
    visitMethodDecl(method);
    method->name = originalName;
  }
  defineSymbol(node->name, new ConstantString(node->name));
}

void NIRBuilder::visitClassDecl(ClassDeclNode *node) {
  auto *c = m_module->createClass(node->name);
  std::unordered_set<std::string> fieldNames;

  for (const auto &baseName : node->baseClasses) {
    for (const auto &existingClass : m_module->getClasses()) {
      if (existingClass->getName() != baseName) {
        continue;
      }
      for (const auto &field : existingClass->getFields()) {
        if (fieldNames.insert(field.name).second) {
          c->addField(field.name, field.type);
        }
      }
    }
  }

  for (auto &member : node->members) {
    if (member->type == ASTNodeType::BindingDecl) {
      auto *b = static_cast<BindingDeclNode *>(member.get());
      NTypePtr fieldType =
          resolveType(b->typeAnnotation ? b->typeAnnotation.get() : nullptr);
      if (fieldType->isAuto() || fieldType->isUnknown()) {
        fieldType = NType::makeInt();
      }
      if (fieldNames.insert(b->name).second) {
        c->addField(b->name, fieldType);
      }
    }
  }

  for (auto &member : node->members) {
    if (member->type == ASTNodeType::MethodDecl) {
      visitMethodDecl(static_cast<MethodDeclNode *>(member.get()));
    }
  }
}

void NIRBuilder::visitMethodDecl(MethodDeclNode *node) {
  NTypePtr retType = resolveType(node->returnType.get());

  Function *f = m_module->createFunction(node->name, retType, node->isExtern);
  m_currentFunction = f;

  for (auto &param : node->parameters) {
    NTypePtr paramType = resolveType(param.typeSpec.get());
    f->addArgument(paramType, param.name);
  }

  if (node->isExtern) {
    m_currentFunction = nullptr;
    m_currentBlock = nullptr;
    return;
  }

  enterScope();
  m_valDefCounter = 0;
  m_blockCounter = 0;
  m_gpuDepth = 0;
  m_gpuRuntimeScopeStack.clear();

  Block *entryBlock = f->createBlock("entry");
  m_currentBlock = entryBlock;

  for (auto &param : node->parameters) {
    Value *argVal = nullptr;
    for (const auto &arg : f->getArguments()) {
      if (arg->getName() == param.name) {
        argVal = arg.get();
        break;
      }
    }
    NTypePtr paramType = resolveType(param.typeSpec.get());
    Instruction *allocaInst = createInst(
        InstKind::Alloca, NType::makePointer(paramType), param.name + "_ptr");
    if (argVal != nullptr) {
      Instruction *storeInst = createInst(InstKind::Store, NType::makeVoid(), "");
      storeInst->addOperand(argVal);
      storeInst->addOperand(allocaInst);
    }
    defineSymbol(param.name, allocaInst);
  }

  if (node->body) {
    visitBlock(static_cast<BlockNode *>(node->body.get()));
  }

  leaveScope();
  m_currentFunction = nullptr;
  m_currentBlock = nullptr;
  m_gpuDepth = 0;
  m_gpuRuntimeScopeStack.clear();
}

void NIRBuilder::visitBindingDecl(BindingDeclNode *node) {
  Value *val = nullptr;
  NTypePtr declaredType =
      node->typeAnnotation ? resolveType(node->typeAnnotation.get())
                           : NType::makeUnknown();
  std::string materialShaderName;
  if (node->value != nullptr && node->value->type == ASTNodeType::CallExpr) {
    auto *call = static_cast<CallExprNode *>(node->value.get());
    if (call->callee != nullptr && call->callee->type == ASTNodeType::TypeSpec) {
      auto *typeSpec = static_cast<TypeSpecNode *>(call->callee.get());
      if (typeSpec->typeName == "Material" && !typeSpec->genericArgs.empty()) {
        materialShaderName =
            resolveCallNameLocal(typeSpec->genericArgs.front().get());
      }
    }
  }
  if (node->name == "__assign__" && node->target) {
    if (node->target->type == ASTNodeType::MemberAccessExpr &&
        node->kind == BindingKind::Value && node->value != nullptr) {
      auto *member = static_cast<MemberAccessNode *>(node->target.get());
      if (member->object != nullptr &&
          member->object->type == ASTNodeType::Identifier) {
        const std::string materialName =
            static_cast<IdentifierNode *>(member->object.get())->name;
        const std::string shaderName = lookupMaterialShader(materialName);
        const std::string runtimeFunc = resolveDescriptorSetFunc(
            m_module.get(), shaderName, member->member);
        if (!runtimeFunc.empty()) {
          Value *materialValue = buildExpression(member->object.get());
          Value *sourceVal = buildExpression(node->value.get());
          if (materialValue != nullptr && sourceVal != nullptr) {
            Instruction *setCall =
                createInst(InstKind::Call, NType::makeVoid(), "");
            setCall->addOperand(new ConstantString(runtimeFunc));
            setCall->addOperand(materialValue);
            setCall->addOperand(new ConstantString(member->member));
            setCall->addOperand(sourceVal);
          }
          return;
        }
      }
    }

    Value *targetPtr = buildLValue(node->target.get());
    Value *sourceVal = nullptr;

    if (node->kind == BindingKind::AddressOf) {
      sourceVal = buildAddressOf(node->value.get());
    } else if (node->kind == BindingKind::ValueOf) {
      Value *operand = buildExpression(node->value.get());
      if (operand) {
        NTypePtr pointee = NType::makeUnknown();
        if (operand->getType() && operand->getType()->kind == TypeKind::Pointer) {
          pointee = operand->getType()->pointeeType;
        }
        Instruction *load = createInst(InstKind::Load, pointee, nextValName());
        load->addOperand(operand);
        sourceVal = load;
      }
    } else {
      sourceVal = buildExpression(node->value.get());
    }

    if (targetPtr && sourceVal) {
      Instruction *store = createInst(InstKind::Store, NType::makeVoid(), "");
      store->addOperand(sourceVal);
      store->addOperand(targetPtr);
    }
    return;
  }

  if (node->kind == BindingKind::AddressOf) {
    val = buildAddressOf(node->value.get());
  } else if (node->kind == BindingKind::ValueOf) {
    Value *operand = buildExpression(node->value.get());
    if (operand) {
      NTypePtr pointee = NType::makeUnknown();
      if (operand->getType() && operand->getType()->kind == TypeKind::Pointer) {
        pointee = operand->getType()->pointeeType;
      }
      Instruction *load = createInst(InstKind::Load, pointee, nextValName());
      load->addOperand(operand);
      val = load;
    }
  } else {
    val = buildExpression(node->value.get());
  }

  Value *ptr = lookupSymbol(node->name);

  if (m_currentFunction) {
    if (!ptr) {
      NTypePtr storageType = declaredType;
      if (!storageType || storageType->isUnknown() || storageType->isAuto() ||
          storageType->isVoid()) {
        storageType = val ? val->getType() : declaredType;
      }
      if (!storageType || storageType->isVoid()) {
        storageType = NType::makeUnknown();
      }
      ptr = createInst(InstKind::Alloca, NType::makePointer(storageType),
                       node->name + "_ptr");
      defineSymbol(node->name, ptr);
    }

    if (val) {
      Instruction *store = createInst(InstKind::Store, NType::makeVoid(), "");
      store->addOperand(val);
      store->addOperand(ptr);
    }
    if (!materialShaderName.empty()) {
      defineMaterialShader(node->name, materialShaderName);
    }
  } else {
    NTypePtr globalType = declaredType;
    if (!globalType || globalType->isUnknown() || globalType->isAuto() ||
        globalType->isVoid()) {
      globalType = val ? val->getType() : declaredType;
    }
    if (!globalType || globalType->isVoid()) {
      globalType = NType::makeUnknown();
    }
    auto *g = m_module->createGlobal(globalType, node->name, val);
    defineSymbol(node->name, g);
    if (!materialShaderName.empty()) {
      defineMaterialShader(node->name, materialShaderName);
    }
  }
}

void NIRBuilder::visitCastStmt(CastStmtNode *node) {
  if (node == nullptr || node->target == nullptr || node->steps.empty()) {
    return;
  }

  NTypePtr finalType = resolveType(node->steps.back().typeSpec.get());
  bool nullable = node->pipelineNullable;
  for (const auto &step : node->steps) {
    nullable = nullable || step.allowNullOnFailure;
  }
  if (nullable) {
    finalType = NType::makeNullable(finalType);
  }
  if (!finalType || finalType->isVoid()) {
    finalType = NType::makeUnknown();
  }

  const bool targetIsIdentifier = node->target->type == ASTNodeType::Identifier;
  const std::string targetName =
      targetIsIdentifier ? static_cast<IdentifierNode *>(node->target.get())->name
                         : std::string();

  if (targetIsIdentifier && lookupSymbol(targetName) == nullptr) {
    if (m_currentFunction) {
      Value *ptr = createInst(InstKind::Alloca, NType::makePointer(finalType),
                              targetName + "_ptr");
      defineSymbol(targetName, ptr);
    } else {
      auto *g = m_module->createGlobal(finalType, targetName, nullptr);
      defineSymbol(targetName, g);
    }
    return;
  }

  Value *sourceVal = buildExpression(node->target.get());
  if (sourceVal != nullptr) {
    Instruction *castInst =
        createInst(InstKind::Cast, finalType, nextValName() + "_cast");
    castInst->addOperand(sourceVal);
    castInst->addOperand(new ConstantString(finalType->toString()));
    sourceVal = castInst;
  }

  if (targetIsIdentifier) {
    if (m_currentFunction) {
      Value *ptr = createInst(InstKind::Alloca, NType::makePointer(finalType),
                              targetName + "_ptr");
      defineSymbol(targetName, ptr);
      if (sourceVal != nullptr) {
        Instruction *store = createInst(InstKind::Store, NType::makeVoid(), "");
        store->addOperand(sourceVal);
        store->addOperand(ptr);
      }
    } else {
      auto *g = m_module->createGlobal(finalType, targetName, sourceVal);
      defineSymbol(targetName, g);
    }
    return;
  }

  Value *targetPtr = buildLValue(node->target.get());
  if (targetPtr != nullptr && sourceVal != nullptr) {
    Instruction *store = createInst(InstKind::Store, NType::makeVoid(), "");
    store->addOperand(sourceVal);
    store->addOperand(targetPtr);
  }
}

} // namespace neuron::nir

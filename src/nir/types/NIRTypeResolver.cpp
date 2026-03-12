#include "neuronc/nir/NIRBuilder.h"

namespace neuron::nir {

NTypePtr NIRBuilder::resolveType(ASTNode *node) {
  if (!node || node->type != ASTNodeType::TypeSpec)
    return NType::makeVoid();

  auto *ts = static_cast<TypeSpecNode *>(node);
  std::string typeName = ts->typeName;

  if (typeName == "int")
    return NType::makeInt();
  if (typeName == "float")
    return NType::makeFloat();
  if (typeName == "double")
    return NType::makeDouble();
  if (typeName == "bool")
    return NType::makeBool();
  if (typeName == "string")
    return NType::makeString();
  if (typeName == "dynamic")
    return NType::makeDynamic();
  if (typeName == "void")
    return NType::makeVoid();
  if (typeName == "shader")
    return NType::makeDynamic();
  if (typeName == "Vector2" || typeName == "Vector3" ||
      typeName == "Vector4" || typeName == "Vec4" ||
      typeName == "Matrix4" || typeName == "Texture2D" ||
      typeName == "Sampler" || typeName == "CommandList") {
    return NType::makeClass(typeName == "Vec4" ? "Vector4" : typeName);
  }
  if (typeName == "Window" || typeName == "Shader" || typeName == "Material" ||
      typeName == "Mesh" || typeName == "Texture2D" || typeName == "Color" ||
      typeName == "CommandList") {
    return NType::makeClass(typeName);
  }

  if (typeName == "Tensor" || typeName == "tensor" || typeName == "Array" ||
      typeName == "array") {
    NTypePtr elemType = NType::makeFloat();
    if (!ts->genericArgs.empty()) {
      if (ts->genericArgs[0]->type == ASTNodeType::Identifier) {
        std::string ename =
            static_cast<IdentifierNode *>(ts->genericArgs[0].get())->name;
        if (ename == "int")
          elemType = NType::makeInt();
        else if (ename == "float")
          elemType = NType::makeFloat();
      }
    }
    if (typeName == "Tensor" || typeName == "tensor")
      return NType::makeTensor(elemType);
    return NType::makeArray(elemType);
  }

  if (typeName == "Dictionary" || typeName == "dictionary") {
    NTypePtr keyType = NType::makeUnknown();
    NTypePtr valueType = NType::makeUnknown();
    auto mapSimpleTypeName = [&](const std::string &n) -> NTypePtr {
      if (n == "int")
        return NType::makeInt();
      if (n == "float")
        return NType::makeFloat();
      if (n == "double")
        return NType::makeDouble();
      if (n == "bool")
        return NType::makeBool();
      if (n == "string")
        return NType::makeString();
      if (n == "dynamic")
        return NType::makeDynamic();
      return NType::makeUnknown();
    };
    if (ts->genericArgs.size() >= 2) {
      if (ts->genericArgs[0]->type == ASTNodeType::Identifier) {
        keyType = mapSimpleTypeName(
            static_cast<IdentifierNode *>(ts->genericArgs[0].get())->name);
      }
      if (ts->genericArgs[1]->type == ASTNodeType::Identifier) {
        valueType = mapSimpleTypeName(
            static_cast<IdentifierNode *>(ts->genericArgs[1].get())->name);
      }
    }
    return NType::makeDictionary(keyType, valueType);
  }

  if (m_enumMembers.find(typeName) != m_enumMembers.end()) {
    return NType::makeEnum(typeName);
  }

  for (const auto &cls : m_module->getClasses()) {
    if (cls->getName() == typeName) {
      return NType::makeClass(typeName);
    }
  }

  return NType::makeUnknown();
}

} // namespace neuron::nir

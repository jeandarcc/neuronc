#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/parser/FusionChain.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace neuron::nir {

namespace {

std::string normalizeModuleNameLocal(std::string name) {
  for (char &ch : name) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return name;
}

NTypePtr nativeBoundaryTypeLocal(const std::string &typeName) {
  if (typeName == "void")
    return NType::makeVoid();
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
  return NType::makeUnknown();
}

std::string resolveCallNameLocal(ASTNode *callee) {
  if (!callee) {
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
    std::string base = resolveCallNameLocal(member->object.get());
    if (base.empty()) {
      return "";
    }
    return base + "." + member->member;
  }
  return "";
}

enum class InputTargetKind {
  None,
  Int,
  Float,
  Double,
  Bool,
  String,
  Enum,
};

struct InputChainInfo {
  TypeSpecNode *inputTypeSpec = nullptr;
  std::vector<CallExprNode *> calls;
  std::vector<std::string> methods;
};

bool collectInputCallChainLocal(CallExprNode *node, InputChainInfo *info) {
  if (node == nullptr || info == nullptr || node->callee == nullptr) {
    return false;
  }

  if (node->callee->type == ASTNodeType::Identifier) {
    auto *identifier = static_cast<IdentifierNode *>(node->callee.get());
    if (identifier->name != "Input") {
      return false;
    }
    info->calls.push_back(node);
    return true;
  }

  if (node->callee->type == ASTNodeType::TypeSpec) {
    auto *typeSpec = static_cast<TypeSpecNode *>(node->callee.get());
    if (typeSpec->typeName != "Input") {
      return false;
    }
    info->inputTypeSpec = typeSpec;
    info->calls.push_back(node);
    return true;
  }

  if (node->callee->type != ASTNodeType::MemberAccessExpr) {
    return false;
  }

  auto *member = static_cast<MemberAccessNode *>(node->callee.get());
  if (member->object == nullptr ||
      member->object->type != ASTNodeType::CallExpr) {
    return false;
  }

  if (!collectInputCallChainLocal(
          static_cast<CallExprNode *>(member->object.get()), info)) {
    return false;
  }

  info->calls.push_back(node);
  info->methods.push_back(member->member);
  return true;
}

std::string inputGenericTypeName(ASTNode *argNode) {
  if (argNode == nullptr) {
    return "";
  }
  if (argNode->type == ASTNodeType::Identifier) {
    return static_cast<IdentifierNode *>(argNode)->name;
  }
  if (argNode->type == ASTNodeType::TypeSpec) {
    return static_cast<TypeSpecNode *>(argNode)->typeName;
  }
  return "";
}

InputTargetKind inputTargetKindFromTypeName(const std::string &typeName) {
  if (typeName == "int") {
    return InputTargetKind::Int;
  }
  if (typeName == "float") {
    return InputTargetKind::Float;
  }
  if (typeName == "double") {
    return InputTargetKind::Double;
  }
  if (typeName == "bool") {
    return InputTargetKind::Bool;
  }
  if (typeName == "string") {
    return InputTargetKind::String;
  }
  return InputTargetKind::None;
}

NTypePtr inputTargetType(InputTargetKind kind, const std::string &enumTypeName) {
  switch (kind) {
  case InputTargetKind::Int:
    return NType::makeInt();
  case InputTargetKind::Float:
    return NType::makeFloat();
  case InputTargetKind::Double:
    return NType::makeDouble();
  case InputTargetKind::Bool:
    return NType::makeBool();
  case InputTargetKind::String:
    return NType::makeString();
  case InputTargetKind::Enum:
    return NType::makeEnum(enumTypeName);
  case InputTargetKind::None:
    break;
  }
  return NType::makeUnknown();
}

const char *inputBuiltinName(InputTargetKind kind) {
  switch (kind) {
  case InputTargetKind::Int:
    return "__neuron_input_int";
  case InputTargetKind::Float:
    return "__neuron_input_float";
  case InputTargetKind::Double:
    return "__neuron_input_double";
  case InputTargetKind::Bool:
    return "__neuron_input_bool";
  case InputTargetKind::String:
    return "__neuron_input_string";
  case InputTargetKind::Enum:
    return "__neuron_input_enum";
  case InputTargetKind::None:
    break;
  }
  return "__neuron_input_unknown";
}

std::string inputEnumOptionsPayload(
    const std::unordered_map<std::string, int64_t> &members) {
  std::vector<std::pair<int64_t, std::string>> ordered;
  ordered.reserve(members.size());
  for (const auto &entry : members) {
    ordered.emplace_back(entry.second, entry.first);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const auto &lhs, const auto &rhs) {
              if (lhs.first != rhs.first) {
                return lhs.first < rhs.first;
              }
              return lhs.second < rhs.second;
            });

  std::ostringstream out;
  for (std::size_t i = 0; i < ordered.size(); ++i) {
    if (i > 0) {
      out << '\n';
    }
    out << ordered[i].second;
  }
  return out.str();
}

} // namespace

Value *NIRBuilder::buildCallExpr(CallExprNode *node) {
  if (const FusionPatternSpec *fusion = matchFusionChain(node)) {
    CallExprNode *baseCall = fusionBaseCall(node);
    if (baseCall == nullptr) {
      reportError(node->location, "Malformed fusion chain call.");
      return nullptr;
    }

    std::vector<Value *> buildArgs;
    buildArgs.reserve(baseCall->arguments.size() + 1);
    for (auto &arg : baseCall->arguments) {
      Value *argVal = buildExpression(arg.get());
      if (argVal == nullptr) {
        reportError(arg ? arg->location : node->location,
                    "Failed to build fused call argument.");
        return nullptr;
      }
      buildArgs.push_back(argVal);
    }

    if (buildArgs.size() != fusion->argumentCount) {
      reportError(node->location,
                  "Fusion chain lowering expected " +
                      std::to_string(fusion->argumentCount) +
                      " arguments but received " +
                      std::to_string(buildArgs.size()) + ".");
      return nullptr;
    }

    Instruction *inst =
        createInst(InstKind::Call, NType::makeTensor(NType::makeFloat()));
    inst->addOperand(new ConstantString(fusion->builtinName));
    for (Value *argVal : buildArgs) {
      inst->addOperand(argVal);
    }
    inst->addOperand(new ConstantInt(kFusionBuiltinPreferGpuExecHint));
    return inst;
  }

  InputChainInfo inputChain;
  if (collectInputCallChainLocal(node, &inputChain)) {
    if (inputChain.calls.empty()) {
      reportError(node->location, "Malformed Input<T> call chain.");
      return nullptr;
    }

    const SourceLocation inputTypeLocation =
        inputChain.inputTypeSpec != nullptr ? inputChain.inputTypeSpec->location
                                            : node->location;
    if (inputChain.inputTypeSpec != nullptr &&
        inputChain.inputTypeSpec->genericArgs.size() > 1) {
      reportError(inputTypeLocation,
                  "Input<T> expects exactly one generic type argument.");
      return nullptr;
    }

    const std::string genericTypeName =
        (inputChain.inputTypeSpec == nullptr ||
         inputChain.inputTypeSpec->genericArgs.empty())
            ? "string"
            : inputGenericTypeName(inputChain.inputTypeSpec->genericArgs[0].get());
    InputTargetKind targetKind = inputTargetKindFromTypeName(genericTypeName);
    if (targetKind == InputTargetKind::None &&
        m_enumMembers.find(genericTypeName) != m_enumMembers.end()) {
      targetKind = InputTargetKind::Enum;
    }
    if (targetKind == InputTargetKind::None) {
      reportError(inputTypeLocation,
                  "Input<T> generic type is not supported in lowering.");
      return nullptr;
    }

    CallExprNode *baseCall = inputChain.calls.front();
    if (baseCall->arguments.size() != 1) {
      reportError(baseCall->location,
                  "Input<T> expects exactly one prompt argument.");
      return nullptr;
    }

    Value *prompt = buildExpression(baseCall->arguments[0].get());
    if (prompt == nullptr) {
      reportError(baseCall->arguments[0]->location,
                  "Failed to lower Input<T> prompt argument.");
      return nullptr;
    }

    Instruction *inst =
        createInst(InstKind::Call, inputTargetType(targetKind, genericTypeName));
    inst->addOperand(new ConstantString(inputBuiltinName(targetKind)));
    inst->addOperand(prompt);

    auto buildSingleArg = [&](CallExprNode *call,
                              const std::string &methodName) -> Value * {
      if (call == nullptr || call->arguments.size() != 1) {
        reportError(call ? call->location : node->location,
                    "Input<T>." + methodName + "() expects one argument.");
        return nullptr;
      }
      return buildExpression(call->arguments[0].get());
    };

    if (targetKind == InputTargetKind::Int) {
      Value *hasMin = new ConstantInt(0);
      Value *minValue = new ConstantInt(0);
      Value *hasMax = new ConstantInt(0);
      Value *maxValue = new ConstantInt(0);
      Value *hasDefault = new ConstantInt(0);
      Value *defaultValue = new ConstantInt(0);
      Value *timeoutMs = new ConstantInt(-1);

      for (std::size_t i = 0; i < inputChain.methods.size(); ++i) {
        const std::string &method = inputChain.methods[i];
        CallExprNode *stageCall = inputChain.calls[i + 1];
        if (method == "Min") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          hasMin = new ConstantInt(1);
          minValue = argValue;
        } else if (method == "Max") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          hasMax = new ConstantInt(1);
          maxValue = argValue;
        } else if (method == "Default") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          hasDefault = new ConstantInt(1);
          defaultValue = argValue;
        } else if (method == "TimeoutMs") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          timeoutMs = argValue;
        } else {
          reportError(stageCall->location,
                      "Unsupported Input<int> fluent method: " + method);
          return nullptr;
        }
      }

      inst->addOperand(hasMin);
      inst->addOperand(minValue);
      inst->addOperand(hasMax);
      inst->addOperand(maxValue);
      inst->addOperand(hasDefault);
      inst->addOperand(defaultValue);
      inst->addOperand(timeoutMs);
      return inst;
    }

    if (targetKind == InputTargetKind::Float ||
        targetKind == InputTargetKind::Double) {
      Value *hasMin = new ConstantInt(0);
      Value *minValue = new ConstantFloat(0.0);
      Value *hasMax = new ConstantInt(0);
      Value *maxValue = new ConstantFloat(0.0);
      Value *hasDefault = new ConstantInt(0);
      Value *defaultValue = new ConstantFloat(0.0);
      Value *timeoutMs = new ConstantInt(-1);

      for (std::size_t i = 0; i < inputChain.methods.size(); ++i) {
        const std::string &method = inputChain.methods[i];
        CallExprNode *stageCall = inputChain.calls[i + 1];
        if (method == "Min") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          hasMin = new ConstantInt(1);
          minValue = argValue;
        } else if (method == "Max") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          hasMax = new ConstantInt(1);
          maxValue = argValue;
        } else if (method == "Default") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          hasDefault = new ConstantInt(1);
          defaultValue = argValue;
        } else if (method == "TimeoutMs") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          timeoutMs = argValue;
        } else {
          reportError(stageCall->location,
                      "Unsupported Input<T> fluent method for floating T: " +
                          method);
          return nullptr;
        }
      }

      inst->addOperand(hasMin);
      inst->addOperand(minValue);
      inst->addOperand(hasMax);
      inst->addOperand(maxValue);
      inst->addOperand(hasDefault);
      inst->addOperand(defaultValue);
      inst->addOperand(timeoutMs);
      return inst;
    }

    if (targetKind == InputTargetKind::Bool) {
      Value *hasDefault = new ConstantInt(0);
      Value *defaultValue = new ConstantInt(0);
      Value *timeoutMs = new ConstantInt(-1);

      for (std::size_t i = 0; i < inputChain.methods.size(); ++i) {
        const std::string &method = inputChain.methods[i];
        CallExprNode *stageCall = inputChain.calls[i + 1];
        if (method == "Default") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          hasDefault = new ConstantInt(1);
          defaultValue = argValue;
        } else if (method == "TimeoutMs") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          timeoutMs = argValue;
        } else {
          reportError(stageCall->location,
                      "Unsupported Input<bool> fluent method: " + method);
          return nullptr;
        }
      }

      inst->addOperand(hasDefault);
      inst->addOperand(defaultValue);
      inst->addOperand(timeoutMs);
      return inst;
    }

    if (targetKind == InputTargetKind::String) {
      Value *secret = new ConstantInt(0);
      Value *hasDefault = new ConstantInt(0);
      Value *defaultValue = new ConstantString("");
      Value *timeoutMs = new ConstantInt(-1);

      for (std::size_t i = 0; i < inputChain.methods.size(); ++i) {
        const std::string &method = inputChain.methods[i];
        CallExprNode *stageCall = inputChain.calls[i + 1];
        if (method == "Secret") {
          if (!stageCall->arguments.empty()) {
            reportError(stageCall->location,
                        "Input<string>.Secret() takes no arguments.");
            return nullptr;
          }
          secret = new ConstantInt(1);
        } else if (method == "Default") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          hasDefault = new ConstantInt(1);
          defaultValue = argValue;
        } else if (method == "TimeoutMs") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          timeoutMs = argValue;
        } else {
          reportError(stageCall->location,
                      "Unsupported Input<string> fluent method: " + method);
          return nullptr;
        }
      }

      inst->addOperand(secret);
      inst->addOperand(hasDefault);
      inst->addOperand(defaultValue);
      inst->addOperand(timeoutMs);
      return inst;
    }

    if (targetKind == InputTargetKind::Enum) {
      auto enumIt = m_enumMembers.find(genericTypeName);
      if (enumIt == m_enumMembers.end()) {
        reportError(inputChain.inputTypeSpec->location,
                    "Input<Enum> could not resolve enum members for '" +
                        genericTypeName + "'.");
        return nullptr;
      }

      Value *optionsPayload =
          new ConstantString(inputEnumOptionsPayload(enumIt->second));
      Value *optionCount =
          new ConstantInt(static_cast<int64_t>(enumIt->second.size()));
      Value *hasDefault = new ConstantInt(0);
      Value *defaultValue = new ConstantInt(0);
      Value *timeoutMs = new ConstantInt(-1);

      for (std::size_t i = 0; i < inputChain.methods.size(); ++i) {
        const std::string &method = inputChain.methods[i];
        CallExprNode *stageCall = inputChain.calls[i + 1];
        if (method == "Default") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          hasDefault = new ConstantInt(1);
          defaultValue = argValue;
        } else if (method == "TimeoutMs") {
          Value *argValue = buildSingleArg(stageCall, method);
          if (argValue == nullptr) {
            return nullptr;
          }
          timeoutMs = argValue;
        } else {
          reportError(stageCall->location,
                      "Unsupported Input<enum> fluent method: " + method);
          return nullptr;
        }
      }

      inst->addOperand(optionsPayload);
      inst->addOperand(optionCount);
      inst->addOperand(hasDefault);
      inst->addOperand(defaultValue);
      inst->addOperand(timeoutMs);
      return inst;
    }

    reportError(node->location, "Unsupported Input<T> lowering target.");
    return nullptr;
  }

  std::string callName = resolveCallNameLocal(node->callee.get());

  if (!callName.empty() && callName.find('.') == std::string::npos) {
    for (const auto &cls : m_module->getClasses()) {
      if (cls->getName() == callName) {
        NTypePtr classType = NType::makeClass(callName);
        return createInst(InstKind::Alloca, NType::makePointer(classType),
                          nextValName() + "_obj");
      }
    }
  }

  std::vector<Value *> buildArgs;
  for (auto &arg : node->arguments) {
    Value *argVal = buildExpression(arg.get());
    if (argVal == nullptr) {
      std::ostringstream oss;
      oss << "Failed to build call argument for '"
          << (callName.empty() ? std::string("<unknown>") : callName) << "'.";
      reportError(arg ? arg->location : node->location, oss.str());
      return nullptr;
    }
    buildArgs.push_back(argVal);
  }

  NTypePtr retType = NType::makeUnknown();
  if (callName == "Random.Float") {
    retType = NType::makeFloat();
  } else if (callName == "Window.Create") {
    retType = NType::makeClass("Window");
  } else if (callName == "Scene.Create") {
    retType = NType::makeClass("Scene");
  } else if (callName == "Renderer2D.Create") {
    retType = NType::makeClass("Renderer2D");
  } else if (callName == "Material") {
    retType = NType::makeClass("Material");
    if (node->callee != nullptr &&
        node->callee->type == ASTNodeType::TypeSpec) {
      auto *typeSpec = static_cast<TypeSpecNode *>(node->callee.get());
      if (!typeSpec->genericArgs.empty()) {
        const std::string shaderName =
            resolveCallNameLocal(typeSpec->genericArgs.front().get());
        if (!shaderName.empty()) {
          Instruction *inst = createInst(InstKind::Call, retType);
          inst->addOperand(new ConstantString("Material.Create"));
          inst->addOperand(new ConstantString(shaderName));
          return inst;
        }
      }
    }
  } else if ((callName.size() >= 8 &&
               callName.rfind(".SetVec4") == callName.size() - 8) ||
             (callName.size() >= 9 &&
              callName.rfind(".SetColor") == callName.size() - 9) ||
             (callName.size() >= 11 &&
              callName.rfind(".SetSampler") == callName.size() - 11) ||
             (callName.size() >= 11 &&
              callName.rfind(".SetMatrix4") == callName.size() - 11) ||
             (callName.size() >= 11 &&
              callName.rfind(".SetTexture") == callName.size() - 11) ||
             (callName.size() >= 8 &&
              callName.rfind(".SetText") == callName.size() - 8) ||
             (callName.size() >= 10 &&
              callName.rfind(".SetParent") == callName.size() - 10) ||
             (callName.size() >= 12 &&
              callName.rfind(".SetPosition") == callName.size() - 12) ||
             (callName.size() >= 12 &&
              callName.rfind(".SetRotation") == callName.size() - 12) ||
             (callName.size() >= 9 &&
              callName.rfind(".SetScale") == callName.size() - 9) ||
             (callName.size() >= 12 &&
              callName.rfind(".SetFontSize") == callName.size() - 12) ||
             (callName.size() >= 13 &&
              callName.rfind(".SetAlignment") == callName.size() - 13) ||
             (callName.size() >= 16 &&
              callName.rfind(".SetSortingLayer") == callName.size() - 16) ||
             (callName.size() >= 16 &&
              callName.rfind(".SetOrderInLayer") == callName.size() - 16) ||
             (callName.size() >= 9 &&
              callName.rfind(".SetZoom") == callName.size() - 8) ||
             (callName.size() >= 11 &&
              callName.rfind(".SetPrimary") == callName.size() - 11) ||
             (callName.size() >= 8 &&
              callName.rfind(".SetSize") == callName.size() - 8) ||
             (callName.size() >= 9 &&
              callName.rfind(".SetPivot") == callName.size() - 9) ||
             (callName.size() >= 9 &&
              callName.rfind(".SetFlipX") == callName.size() - 9) ||
             (callName.size() >= 9 &&
              callName.rfind(".SetFlipY") == callName.size() - 9) ||
             (callName.size() >= 10 &&
              callName.rfind(".SetCamera") == callName.size() - 10) ||
             (callName.size() >= 14 &&
              callName.rfind(".SetClearColor") == callName.size() - 14) ||
             (callName.size() >= 7 &&
              callName.rfind(".Render") == callName.size() - 7) ||
             (callName.size() >= 10 &&
              callName.rfind(".SetFilled") == callName.size() - 10) ||
             (callName.size() >= 10 &&
              callName.rfind(".SetCircle") == callName.size() - 10) ||
             (callName.size() >= 8 &&
              callName.rfind(".SetLine") == callName.size() - 8) ||
             (callName.size() >= 13 &&
              callName.rfind(".SetRectangle") == callName.size() - 13) ||
             (callName.size() >= 8 &&
              callName.rfind(".SetFont") == callName.size() - 8)) {
    retType = NType::makeVoid();
  } else if (callName == "Texture2D.Load") {
    retType = NType::makeClass("Texture2D");
  } else if (callName == "Font.Load") {
    retType = NType::makeClass("Font");
  } else if (callName == "Sampler.Create") {
    retType = NType::makeClass("Sampler");
  } else if (callName == "Mesh.Load") {
    retType = NType::makeClass("Mesh");
  } else if (callName == "Vector2") {
    retType = NType::makeClass("Vector2");
  } else if (callName == "Vector3") {
    retType = NType::makeClass("Vector3");
  } else if (callName == "Vector4") {
    retType = NType::makeClass("Vector4");
  } else if (callName == "Graphics.CreateCanvas") {
    retType = NType::makeDynamic();
  } else if (callName == "cmd.Draw" || callName == "cmd.DrawIndexed" ||
             callName == "cmd.DrawInstanced" || callName == "cmd.Clear" ||
             callName == "Present") {
    retType = NType::makeVoid();
  } else if (callName == "Color") {
    retType = NType::makeClass("Color");
  } else if (callName.size() >= 13 &&
             callName.rfind(".CreateEntity") == callName.size() - 13) {
    retType = NType::makeClass("Entity");
  } else if (callName.size() >= 11 &&
             callName.rfind(".FindEntity") == callName.size() - 11) {
    retType = NType::makeClass("Entity");
  } else if (callName.size() >= 13 &&
             callName.rfind(".GetTransform") == callName.size() - 13) {
    retType = NType::makeClass("Transform");
  } else if (callName.size() >= 12 &&
             callName.rfind(".AddCamera2D") == callName.size() - 12) {
    retType = NType::makeClass("Camera2D");
  } else if (callName.size() >= 20 &&
             callName.rfind(".AddSpriteRenderer2D") == callName.size() - 20) {
    retType = NType::makeClass("SpriteRenderer2D");
  } else if (callName.size() >= 19 &&
             callName.rfind(".AddShapeRenderer2D") == callName.size() - 19) {
    retType = NType::makeClass("ShapeRenderer2D");
  } else if (callName.size() >= 18 &&
             callName.rfind(".AddTextRenderer2D") == callName.size() - 18) {
    retType = NType::makeClass("TextRenderer2D");
  } else if (callName == "thread") {
    retType = NType::makeInt();
  } else if (callName == "Random.Int" || callName == "Time.Now" ||
             callName == "IO.ReadInt") {
    retType = NType::makeInt();
  } else if (callName == "Resource.Exists") {
    retType = NType::makeInt();
  } else if (callName == "Resource.ReadText") {
    retType = NType::makeString();
  } else if (callName == "Resource.ReadBytes") {
    retType = NType::makeArray(NType::makeInt());
  } else if (callName == "Tensor.Random" || callName == "Tensor.Zeros" ||
             callName == "Tensor.Ones" || callName == "Tensor.Identity" ||
             callName == "create_tensor") {
    retType = NType::makeTensor(NType::makeFloat());
  } else if (callName.find('.') != std::string::npos) {
    const size_t dot = callName.find('.');
    const std::string moduleName =
        normalizeModuleNameLocal(callName.substr(0, dot));
    const std::string exportName = callName.substr(dot + 1);
    auto moduleIt = m_moduleCppModules.find(moduleName);
    if (moduleIt != m_moduleCppModules.end()) {
      auto exportIt = moduleIt->second.exports.find(exportName);
      if (exportIt != moduleIt->second.exports.end()) {
        retType = nativeBoundaryTypeLocal(exportIt->second.returnTypeName);
      }
    }
    if (retType->isUnknown()) {
      for (const auto &fn : m_module->getFunctions()) {
        if (fn->getName() == callName) {
          retType = fn->getReturnType();
          break;
        }
      }
    }
  } else if (!callName.empty()) {
    for (const auto &fn : m_module->getFunctions()) {
      if (fn->getName() == callName) {
        retType = fn->getReturnType();
        break;
      }
    }
  }

  Instruction *inst = createInst(InstKind::Call, retType);
  inst->addOperand(new ConstantString(callName.empty() ? "__unknown__" : callName));
  for (auto *argVal : buildArgs) {
    inst->addOperand(argVal);
  }
  return inst;
}

Value *NIRBuilder::buildInputExpr(InputExprNode *node) {
  if (node == nullptr) {
    return nullptr;
  }

  const SourceLocation inputTypeLocation =
      !node->typeArguments.empty() ? node->typeArguments.front()->location
                                   : node->location;
  if (node->typeArguments.size() > 1) {
    reportError(inputTypeLocation,
                "Input<T> expects exactly one generic type argument.");
    return nullptr;
  }

  const std::string genericTypeName =
      node->typeArguments.empty() ? "string"
                                  : inputGenericTypeName(
                                        node->typeArguments.front().get());
  InputTargetKind targetKind = inputTargetKindFromTypeName(genericTypeName);
  if (targetKind == InputTargetKind::None &&
      m_enumMembers.find(genericTypeName) != m_enumMembers.end()) {
    targetKind = InputTargetKind::Enum;
  }
  if (targetKind == InputTargetKind::None) {
    reportError(inputTypeLocation,
                "Input<T> generic type is not supported in lowering.");
    return nullptr;
  }

  if (node->promptArguments.size() != 1) {
    reportError(node->location, "Input<T> expects exactly one prompt argument.");
    return nullptr;
  }

  Value *prompt = buildExpression(node->promptArguments.front().get());
  if (prompt == nullptr) {
    reportError(node->promptArguments.front()->location,
                "Failed to lower Input<T> prompt argument.");
    return nullptr;
  }

  Instruction *inst =
      createInst(InstKind::Call, inputTargetType(targetKind, genericTypeName));
  inst->addOperand(new ConstantString(inputBuiltinName(targetKind)));
  inst->addOperand(prompt);

  auto buildSingleArg = [&](const InputStageNode &stage) -> Value * {
    if (stage.arguments.size() != 1) {
      reportError(stage.location,
                  "Input<T>." + stage.method + "() expects one argument.");
      return nullptr;
    }
    return buildExpression(stage.arguments.front().get());
  };

  if (targetKind == InputTargetKind::Int) {
    Value *hasMin = new ConstantInt(0);
    Value *minValue = new ConstantInt(0);
    Value *hasMax = new ConstantInt(0);
    Value *maxValue = new ConstantInt(0);
    Value *hasDefault = new ConstantInt(0);
    Value *defaultValue = new ConstantInt(0);
    Value *timeoutMs = new ConstantInt(-1);

    for (const auto &stage : node->stages) {
      if (stage.method == "Min") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        hasMin = new ConstantInt(1);
        minValue = argValue;
      } else if (stage.method == "Max") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        hasMax = new ConstantInt(1);
        maxValue = argValue;
      } else if (stage.method == "Default") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        hasDefault = new ConstantInt(1);
        defaultValue = argValue;
      } else if (stage.method == "TimeoutMs") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        timeoutMs = argValue;
      } else {
        reportError(stage.location,
                    "Unsupported Input<int> fluent method: " + stage.method);
        return nullptr;
      }
    }

    inst->addOperand(hasMin);
    inst->addOperand(minValue);
    inst->addOperand(hasMax);
    inst->addOperand(maxValue);
    inst->addOperand(hasDefault);
    inst->addOperand(defaultValue);
    inst->addOperand(timeoutMs);
    return inst;
  }

  if (targetKind == InputTargetKind::Float ||
      targetKind == InputTargetKind::Double) {
    Value *hasMin = new ConstantInt(0);
    Value *minValue = new ConstantFloat(0.0);
    Value *hasMax = new ConstantInt(0);
    Value *maxValue = new ConstantFloat(0.0);
    Value *hasDefault = new ConstantInt(0);
    Value *defaultValue = new ConstantFloat(0.0);
    Value *timeoutMs = new ConstantInt(-1);

    for (const auto &stage : node->stages) {
      if (stage.method == "Min") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        hasMin = new ConstantInt(1);
        minValue = argValue;
      } else if (stage.method == "Max") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        hasMax = new ConstantInt(1);
        maxValue = argValue;
      } else if (stage.method == "Default") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        hasDefault = new ConstantInt(1);
        defaultValue = argValue;
      } else if (stage.method == "TimeoutMs") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        timeoutMs = argValue;
      } else {
        reportError(stage.location,
                    "Unsupported Input<T> fluent method for floating T: " +
                        stage.method);
        return nullptr;
      }
    }

    inst->addOperand(hasMin);
    inst->addOperand(minValue);
    inst->addOperand(hasMax);
    inst->addOperand(maxValue);
    inst->addOperand(hasDefault);
    inst->addOperand(defaultValue);
    inst->addOperand(timeoutMs);
    return inst;
  }

  if (targetKind == InputTargetKind::Bool) {
    Value *hasDefault = new ConstantInt(0);
    Value *defaultValue = new ConstantInt(0);
    Value *timeoutMs = new ConstantInt(-1);

    for (const auto &stage : node->stages) {
      if (stage.method == "Default") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        hasDefault = new ConstantInt(1);
        defaultValue = argValue;
      } else if (stage.method == "TimeoutMs") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        timeoutMs = argValue;
      } else {
        reportError(stage.location,
                    "Unsupported Input<bool> fluent method: " + stage.method);
        return nullptr;
      }
    }

    inst->addOperand(hasDefault);
    inst->addOperand(defaultValue);
    inst->addOperand(timeoutMs);
    return inst;
  }

  if (targetKind == InputTargetKind::String) {
    Value *secret = new ConstantInt(0);
    Value *hasDefault = new ConstantInt(0);
    Value *defaultValue = new ConstantString("");
    Value *timeoutMs = new ConstantInt(-1);

    for (const auto &stage : node->stages) {
      if (stage.method == "Secret") {
        if (!stage.arguments.empty()) {
          reportError(stage.location,
                      "Input<string>.Secret() takes no arguments.");
          return nullptr;
        }
        secret = new ConstantInt(1);
      } else if (stage.method == "Default") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        hasDefault = new ConstantInt(1);
        defaultValue = argValue;
      } else if (stage.method == "TimeoutMs") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        timeoutMs = argValue;
      } else {
        reportError(stage.location,
                    "Unsupported Input<string> fluent method: " + stage.method);
        return nullptr;
      }
    }

    inst->addOperand(secret);
    inst->addOperand(hasDefault);
    inst->addOperand(defaultValue);
    inst->addOperand(timeoutMs);
    return inst;
  }

  if (targetKind == InputTargetKind::Enum) {
    auto enumIt = m_enumMembers.find(genericTypeName);
    if (enumIt == m_enumMembers.end()) {
      reportError(inputTypeLocation,
                  "Input<Enum> could not resolve enum members for '" +
                      genericTypeName + "'.");
      return nullptr;
    }

    Value *optionsPayload =
        new ConstantString(inputEnumOptionsPayload(enumIt->second));
    Value *optionCount =
        new ConstantInt(static_cast<int64_t>(enumIt->second.size()));
    Value *hasDefault = new ConstantInt(0);
    Value *defaultValue = new ConstantInt(0);
    Value *timeoutMs = new ConstantInt(-1);

    for (const auto &stage : node->stages) {
      if (stage.method == "Default") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        hasDefault = new ConstantInt(1);
        defaultValue = argValue;
      } else if (stage.method == "TimeoutMs") {
        Value *argValue = buildSingleArg(stage);
        if (argValue == nullptr) {
          return nullptr;
        }
        timeoutMs = argValue;
      } else {
        reportError(stage.location,
                    "Unsupported Input<enum> fluent method: " + stage.method);
        return nullptr;
      }
    }

    inst->addOperand(optionsPayload);
    inst->addOperand(optionCount);
    inst->addOperand(hasDefault);
    inst->addOperand(defaultValue);
    inst->addOperand(timeoutMs);
    return inst;
  }

  reportError(node->location, "Unsupported Input<T> lowering target.");
  return nullptr;
}

} // namespace neuron::nir

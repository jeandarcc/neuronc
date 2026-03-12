#include "VMInternal.h"

namespace neuron::ncon::detail {

const std::string &Executor::stringAt(uint32_t id) const {
  static const std::string kEmpty;
  if (id == kInvalidIndex || id >= program().strings.size()) {
    return kEmpty;
  }
  return program().strings[id];
}

const TypeRecord *Executor::typeAt(uint32_t id) const {
  if (id == kInvalidIndex || id >= program().types.size()) {
    return nullptr;
  }
  return &program().types[id];
}

VMValue Executor::defaultValue(uint32_t typeId) const {
  VMValue value;
  const TypeRecord *type = typeAt(typeId);
  if (type == nullptr) {
    value.data = std::monostate{};
    return value;
  }
  switch (type->kind) {
  case TypeKind::Int:
  case TypeKind::Bool:
  case TypeKind::Enum:
    value.data = int64_t{0};
    break;
  case TypeKind::Float:
  case TypeKind::Double:
    value.data = 0.0;
    break;
  case TypeKind::String:
    value.data = std::string();
    break;
  case TypeKind::Nullable:
    value.data = std::monostate{};
    break;
  case TypeKind::Pointer:
    value.data = PointerHandle{};
    break;
  case TypeKind::Array:
    value.data = std::make_shared<std::vector<int64_t>>();
    break;
  case TypeKind::Tensor:
    value.data = TensorHandle(nullptr);
    break;
  case TypeKind::Class: {
    auto object = std::make_shared<ClassObject>();
    for (size_t i = 0; i < type->fieldTypeIds.size(); ++i) {
      auto cell = std::make_shared<Cell>();
      cell->typeId = type->fieldTypeIds[i];
      cell->value = defaultValue(cell->typeId);
      object->fields.push_back(cell);
    }
    value.data = object;
    break;
  }
  default:
    value.data = std::monostate{};
    break;
  }
  return value;
}

VMValue Executor::constantValue(uint32_t constantId) const {
  VMValue value;
  if (constantId >= program().constants.size()) {
    value.data = std::monostate{};
    return value;
  }
  const ConstantRecord &record = program().constants[constantId];
  switch (record.kind) {
  case ConstantKind::Int:
    value.data = record.intValue;
    break;
  case ConstantKind::Float:
    value.data = record.floatValue;
    break;
  case ConstantKind::String:
    value.data = stringAt(record.stringId);
    break;
  }
  return value;
}

int64_t Executor::toInt(const VMValue &value) const {
  if (const auto *v = std::get_if<int64_t>(&value.data)) {
    return *v;
  }
  if (const auto *v = std::get_if<double>(&value.data)) {
    return static_cast<int64_t>(*v);
  }
  if (const auto *v = std::get_if<std::string>(&value.data)) {
    return v->empty() ? 0 : 1;
  }
  if (const auto *v = std::get_if<PointerHandle>(&value.data)) {
    return *v ? 1 : 0;
  }
  if (const auto *v = std::get_if<ClassObjectHandle>(&value.data)) {
    return *v ? 1 : 0;
  }
  if (const auto *v = std::get_if<TensorHandle>(&value.data)) {
    return *v == nullptr ? 0 : 1;
  }
  if (const auto *v = std::get_if<ArrayIntHandle>(&value.data)) {
    return (*v && !(*v)->empty()) ? 1 : 0;
  }
  return 0;
}

double Executor::toDouble(const VMValue &value) const {
  if (const auto *v = std::get_if<double>(&value.data)) {
    return *v;
  }
  if (const auto *v = std::get_if<int64_t>(&value.data)) {
    return static_cast<double>(*v);
  }
  return 0.0;
}

bool Executor::isStringValue(const VMValue &value) const {
  return std::holds_alternative<std::string>(value.data);
}

bool Executor::isFloatingValue(const VMValue &value) const {
  return std::holds_alternative<double>(value.data);
}

std::string Executor::toString(const VMValue &value) const {
  if (const auto *v = std::get_if<std::string>(&value.data)) {
    return *v;
  }
  if (const auto *v = std::get_if<int64_t>(&value.data)) {
    return std::to_string(*v);
  }
  if (const auto *v = std::get_if<double>(&value.data)) {
    return std::to_string(*v);
  }
  if (const auto *v = std::get_if<PointerHandle>(&value.data)) {
    return *v ? "<ptr>" : "<null>";
  }
  if (const auto *v = std::get_if<ClassObjectHandle>(&value.data)) {
    return *v ? "<object>" : "<null object>";
  }
  if (const auto *v = std::get_if<TensorHandle>(&value.data)) {
    return *v == nullptr ? "<null tensor>" : "<tensor>";
  }
  if (const auto *v = std::get_if<ArrayIntHandle>(&value.data)) {
    return *v ? ("<array size=" + std::to_string((*v)->size()) + ">")
              : "<null array>";
  }
  return "";
}

PointerHandle Executor::toPointer(const VMValue &value) const {
  if (const auto *v = std::get_if<PointerHandle>(&value.data)) {
    return *v;
  }
  return {};
}

TensorHandle Executor::toTensor(const VMValue &value) const {
  if (const auto *v = std::get_if<TensorHandle>(&value.data)) {
    return *v;
  }
  return nullptr;
}

bool Executor::truthy(const VMValue &value) const { return toInt(value) != 0; }

bool Executor::tryParseInt(const std::string &text, int64_t *outValue) {
  try {
    size_t parsed = 0;
    const long long value = std::stoll(text, &parsed);
    if (parsed != text.size()) {
      return false;
    }
    if (outValue != nullptr) {
      *outValue = static_cast<int64_t>(value);
    }
    return true;
  } catch (...) {
    return false;
  }
}

bool Executor::tryParseDouble(const std::string &text, double *outValue) {
  try {
    size_t parsed = 0;
    const double value = std::stod(text, &parsed);
    if (parsed != text.size()) {
      return false;
    }
    if (outValue != nullptr) {
      *outValue = value;
    }
    return true;
  } catch (...) {
    return false;
  }
}

bool Executor::tryCastValue(const VMValue &value, uint32_t typeId, VMValue *outValue,
                            std::string *outError) const {
  if (outValue == nullptr) {
    return false;
  }

  const TypeRecord *type = typeAt(typeId);
  if (type == nullptr) {
    *outValue = value;
    return true;
  }

  if (type->kind == TypeKind::Nullable) {
    if (std::holds_alternative<std::monostate>(value.data)) {
      outValue->data = std::monostate{};
      return true;
    }

    const uint32_t baseTypeId =
        type->genericTypeIds.empty() ? kInvalidIndex : type->genericTypeIds.front();
    if (baseTypeId == kInvalidIndex) {
      outValue->data = std::monostate{};
      return true;
    }

    VMValue nestedValue;
    if (tryCastValue(value, baseTypeId, &nestedValue, nullptr)) {
      *outValue = std::move(nestedValue);
      return true;
    }

    outValue->data = std::monostate{};
    return true;
  }

  if (std::holds_alternative<std::monostate>(value.data)) {
    if (outError != nullptr) {
      *outError = "ncon cast from null to non-nullable " +
                  canonicalTypeKey(program(), typeId, nullptr);
    }
    return false;
  }

  switch (type->kind) {
  case TypeKind::Dynamic:
    *outValue = value;
    return true;
  case TypeKind::Int:
  case TypeKind::Enum: {
    if (const auto *v = std::get_if<int64_t>(&value.data)) {
      outValue->data = *v;
      return true;
    }
    if (const auto *v = std::get_if<double>(&value.data)) {
      outValue->data = static_cast<int64_t>(*v);
      return true;
    }
    if (const auto *v = std::get_if<std::string>(&value.data)) {
      int64_t parsed = 0;
      if (tryParseInt(*v, &parsed)) {
        outValue->data = parsed;
        return true;
      }
    }
    break;
  }
  case TypeKind::Bool: {
    if (const auto *v = std::get_if<int64_t>(&value.data)) {
      outValue->data = int64_t{*v != 0};
      return true;
    }
    if (const auto *v = std::get_if<double>(&value.data)) {
      outValue->data = int64_t{*v != 0.0};
      return true;
    }
    if (const auto *v = std::get_if<std::string>(&value.data)) {
      if (*v == "true") {
        outValue->data = int64_t{1};
        return true;
      }
      if (*v == "false") {
        outValue->data = int64_t{0};
        return true;
      }
    }
    break;
  }
  case TypeKind::Float:
  case TypeKind::Double: {
    if (const auto *v = std::get_if<double>(&value.data)) {
      outValue->data = *v;
      return true;
    }
    if (const auto *v = std::get_if<int64_t>(&value.data)) {
      outValue->data = static_cast<double>(*v);
      return true;
    }
    if (const auto *v = std::get_if<std::string>(&value.data)) {
      double parsed = 0.0;
      if (tryParseDouble(*v, &parsed)) {
        outValue->data = parsed;
        return true;
      }
    }
    break;
  }
  case TypeKind::String:
    outValue->data = toString(value);
    return true;
  case TypeKind::Pointer:
    if (const auto *v = std::get_if<PointerHandle>(&value.data)) {
      outValue->data = *v;
      return true;
    }
    break;
  case TypeKind::Array:
    if (const auto *v = std::get_if<ArrayIntHandle>(&value.data)) {
      outValue->data = *v;
      return true;
    }
    break;
  case TypeKind::Tensor:
    if (const auto *v = std::get_if<TensorHandle>(&value.data)) {
      outValue->data = *v;
      return true;
    }
    break;
  case TypeKind::Class:
    if (const auto *v = std::get_if<ClassObjectHandle>(&value.data)) {
      outValue->data = *v;
      return true;
    }
    break;
  default:
    break;
  }

  if (outError != nullptr) {
    *outError = "ncon cast failed to " + canonicalTypeKey(program(), typeId, nullptr);
  }
  return false;
}

VMValue Executor::coerce(const VMValue &value, uint32_t typeId) const {
  VMValue coerced;
  const TypeRecord *type = typeAt(typeId);
  if (type == nullptr) {
    return value;
  }
  switch (type->kind) {
  case TypeKind::Int:
  case TypeKind::Bool:
  case TypeKind::Enum:
    coerced.data = toInt(value);
    break;
  case TypeKind::Float:
  case TypeKind::Double:
    coerced.data = toDouble(value);
    break;
  case TypeKind::String:
    coerced.data = toString(value);
    break;
  case TypeKind::Pointer:
    coerced.data = toPointer(value);
    break;
  case TypeKind::Array:
    if (const auto *array = std::get_if<ArrayIntHandle>(&value.data)) {
      coerced.data = *array;
    } else {
      coerced.data = std::make_shared<std::vector<int64_t>>();
    }
    break;
  case TypeKind::Tensor:
    coerced.data = toTensor(value);
    break;
  default:
    return value;
  }
  return coerced;
}

VMValue Executor::operandValue(const Frame &frame, const OperandRecord &operand) const {
  VMValue value;
  switch (operand.kind) {
  case OperandKind::Slot:
    if (operand.value < frame.slots.size()) {
      value = frame.slots[operand.value];
    }
    break;
  case OperandKind::Constant:
    value = constantValue(operand.value);
    break;
  case OperandKind::Global:
    if (operand.value < m_globals.size()) {
      value.data = m_globals[operand.value];
    }
    break;
  default:
    value.data = std::monostate{};
    break;
  }
  return value;
}

const OperandRecord &Executor::operandAt(const InstructionRecord &instruction,
                                         uint32_t index) const {
  return program().operands[instruction.operandBegin + index];
}

} // namespace neuron::ncon::detail


#include "neuron_runtime.h"
#include "neuron_platform.h"

#include <ffi.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


namespace fs = std::filesystem;

namespace {

enum class ModuleCppTypeKind {
  Void,
  Int,
  Float,
  Double,
  Bool,
  String,
};

struct LoadedLibrary {
  fs::path path;
  NeuronPlatformLibraryHandle handle = nullptr;
};

struct ExportBinding {
  std::string callTarget;
  std::string libraryPath;
  std::string symbolName;
  std::vector<std::string> parameterTypes;
  std::string returnType = "void";
  ModuleCppTypeKind returnKind = ModuleCppTypeKind::Void;
  std::vector<ModuleCppTypeKind> parameterKinds;
  void *symbol = nullptr;
  ffi_cif cif{};
  std::vector<ffi_type *> ffiArgTypes;
  bool prepared = false;
};

std::unordered_map<std::string, ExportBinding> g_bindings;
std::unordered_map<std::string, LoadedLibrary> g_libraries;
std::string g_lastReturnedString;

fs::path executableDirectory() {
  char buffer[4096] = {};
  if (neuron_platform_current_executable_path(buffer, sizeof(buffer))) {
    return fs::path(buffer).parent_path();
  }
  return fs::current_path();
}

void setModuleCppError(const std::string &message) { neuron_throw(message.c_str()); }

ModuleCppTypeKind typeKindFromName(const std::string &typeName) {
  if (typeName == "void") {
    return ModuleCppTypeKind::Void;
  }
  if (typeName == "int") {
    return ModuleCppTypeKind::Int;
  }
  if (typeName == "float") {
    return ModuleCppTypeKind::Float;
  }
  if (typeName == "double") {
    return ModuleCppTypeKind::Double;
  }
  if (typeName == "bool") {
    return ModuleCppTypeKind::Bool;
  }
  if (typeName == "string") {
    return ModuleCppTypeKind::String;
  }
  return ModuleCppTypeKind::Void;
}

bool parseTypeCsv(const char *csv, std::vector<std::string> *outTypes) {
  if (outTypes == nullptr) {
    return false;
  }
  outTypes->clear();
  if (csv == nullptr || *csv == '\0') {
    return true;
  }

  std::string current;
  for (const char *cursor = csv; *cursor != '\0'; ++cursor) {
    if (*cursor == ',') {
      if (!current.empty()) {
        outTypes->push_back(current);
        current.clear();
      }
      continue;
    }
    if (*cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n') {
      current.push_back(*cursor);
    }
  }
  if (!current.empty()) {
    outTypes->push_back(current);
  }
  return true;
}

ffi_type *ffiTypeFor(ModuleCppTypeKind kind) {
  switch (kind) {
  case ModuleCppTypeKind::Void:
    return &ffi_type_void;
  case ModuleCppTypeKind::Int:
    return &ffi_type_sint32;
  case ModuleCppTypeKind::Float:
    return &ffi_type_float;
  case ModuleCppTypeKind::Double:
    return &ffi_type_double;
  case ModuleCppTypeKind::Bool:
    return &ffi_type_uint8;
  case ModuleCppTypeKind::String:
    return &ffi_type_pointer;
  }
  return &ffi_type_void;
}

fs::path resolveLibraryPath(const std::string &libraryPath) {
  fs::path path(libraryPath);
  if (path.is_absolute()) {
    return path;
  }
  return (executableDirectory() / path).lexically_normal();
}

bool ensureLibraryLoaded(const fs::path &path, LoadedLibrary **outLibrary) {
  if (outLibrary == nullptr) {
    return false;
  }
  auto existing = g_libraries.find(path.string());
  if (existing != g_libraries.end()) {
    *outLibrary = &existing->second;
    return true;
  }

  LoadedLibrary library;
  library.path = path;
  library.handle = neuron_platform_open_library(path.string().c_str());
  if (library.handle == nullptr) {
    setModuleCppError("failed to load modulecpp library: " + path.string() +
                      " (" + (neuron_platform_last_error() ? neuron_platform_last_error() : "unknown") + ")");
    return false;
  }

  auto inserted = g_libraries.emplace(path.string(), std::move(library));
  *outLibrary = &inserted.first->second;
  return true;
}

bool prepareBinding(ExportBinding *binding) {
  if (binding == nullptr) {
    setModuleCppError("modulecpp internal error: null binding");
    return false;
  }
  if (binding->prepared) {
    return true;
  }

  binding->returnKind = typeKindFromName(binding->returnType);
  if (binding->returnType != "void" && binding->returnKind == ModuleCppTypeKind::Void) {
    setModuleCppError("unsupported modulecpp return type: " + binding->returnType);
    return false;
  }

  binding->parameterKinds.clear();
  binding->ffiArgTypes.clear();
  for (const auto &typeName : binding->parameterTypes) {
    const ModuleCppTypeKind kind = typeKindFromName(typeName);
    if (kind == ModuleCppTypeKind::Void) {
      setModuleCppError("unsupported modulecpp parameter type: " + typeName);
      return false;
    }
    binding->parameterKinds.push_back(kind);
    binding->ffiArgTypes.push_back(ffiTypeFor(kind));
  }

  LoadedLibrary *library = nullptr;
  if (!ensureLibraryLoaded(resolveLibraryPath(binding->libraryPath), &library)) {
    return false;
  }

  binding->symbol = neuron_platform_load_symbol(library->handle, binding->symbolName.c_str());
  if (binding->symbol == nullptr) {
    setModuleCppError("modulecpp symbol not found: " + binding->symbolName);
    return false;
  }

  if (ffi_prep_cif(&binding->cif, FFI_DEFAULT_ABI,
                   static_cast<unsigned int>(binding->ffiArgTypes.size()),
                   ffiTypeFor(binding->returnKind), binding->ffiArgTypes.data()) !=
      FFI_OK) {
    setModuleCppError("failed to prepare ffi call interface for " +
                      binding->callTarget);
    return false;
  }

  binding->prepared = true;
  return true;
}

struct ArgumentStorage {
  int32_t intValue = 0;
  float floatValue = 0.0f;
  double doubleValue = 0.0;
  std::uint8_t boolValue = 0;
  const char *stringValue = nullptr;
  std::string stringStorage;
};

} // namespace

extern "C" int64_t neuron_modulecpp_register(const char *call_target,
                                             const char *library_path,
                                             const char *symbol_name,
                                             const char *parameter_types_csv,
                                             const char *return_type) {
  if (call_target == nullptr || *call_target == '\0' || library_path == nullptr ||
      *library_path == '\0' || symbol_name == nullptr || *symbol_name == '\0' ||
      return_type == nullptr || *return_type == '\0') {
    setModuleCppError("modulecpp registration requires non-empty metadata");
    return 0;
  }

  ExportBinding binding;
  binding.callTarget = call_target;
  binding.libraryPath = library_path;
  binding.symbolName = symbol_name;
  binding.returnType = return_type;
  if (!parseTypeCsv(parameter_types_csv, &binding.parameterTypes)) {
    setModuleCppError("failed to parse modulecpp parameter signature");
    return 0;
  }

  g_bindings[binding.callTarget] = std::move(binding);
  return 1;
}

extern "C" int64_t neuron_modulecpp_startup(void) {
  for (auto &entry : g_bindings) {
    if (!prepareBinding(&entry.second)) {
      return 0;
    }
  }
  return 1;
}

extern "C" void neuron_modulecpp_shutdown(void) {
  for (auto &entry : g_bindings) {
    entry.second.prepared = false;
    entry.second.symbol = nullptr;
    entry.second.ffiArgTypes.clear();
  }
  for (auto &entry : g_libraries) {
    if (entry.second.handle != nullptr) {
      neuron_platform_close_library(entry.second.handle);
    }
  }
  g_libraries.clear();
  g_lastReturnedString.clear();
}

extern "C" int64_t neuron_modulecpp_invoke(const char *call_target,
                                           const NeuronModuleCppValue *args,
                                           int64_t arg_count,
                                           NeuronModuleCppValue *out_value) {
  if (call_target == nullptr || *call_target == '\0') {
    setModuleCppError("modulecpp invoke requires a call target");
    return 0;
  }

  auto bindingIt = g_bindings.find(call_target);
  if (bindingIt == g_bindings.end()) {
    setModuleCppError(std::string("modulecpp call target not registered: ") +
                      call_target);
    return 0;
  }
  ExportBinding &binding = bindingIt->second;
  if (!prepareBinding(&binding)) {
    return 0;
  }
  if (arg_count < 0 ||
      static_cast<size_t>(arg_count) != binding.parameterKinds.size()) {
    setModuleCppError(std::string("modulecpp argument count mismatch for ") +
                      call_target);
    return 0;
  }

  std::vector<ArgumentStorage> storage(static_cast<size_t>(arg_count));
  std::vector<void *> argPointers(static_cast<size_t>(arg_count), nullptr);
  for (size_t i = 0; i < static_cast<size_t>(arg_count); ++i) {
    const NeuronModuleCppValue &value = args[i];
    ArgumentStorage &slot = storage[i];
    switch (binding.parameterKinds[i]) {
    case ModuleCppTypeKind::Int:
      slot.intValue = static_cast<int32_t>(value.int_value);
      argPointers[i] = &slot.intValue;
      break;
    case ModuleCppTypeKind::Float:
      slot.floatValue = static_cast<float>(value.float_value);
      argPointers[i] = &slot.floatValue;
      break;
    case ModuleCppTypeKind::Double:
      slot.doubleValue = value.float_value;
      argPointers[i] = &slot.doubleValue;
      break;
    case ModuleCppTypeKind::Bool:
      slot.boolValue = value.int_value != 0 ? 1u : 0u;
      argPointers[i] = &slot.boolValue;
      break;
    case ModuleCppTypeKind::String:
      slot.stringStorage = value.string_value == nullptr ? "" : value.string_value;
      slot.stringValue = slot.stringStorage.c_str();
      argPointers[i] = &slot.stringValue;
      break;
    case ModuleCppTypeKind::Void:
      setModuleCppError("invalid void parameter in modulecpp invoke");
      return 0;
    }
  }

  if (out_value != nullptr) {
    out_value->kind = NEURON_MODULECPP_KIND_VOID;
    out_value->reserved = 0;
    out_value->int_value = 0;
    out_value->float_value = 0.0;
    out_value->string_value = nullptr;
  }

  switch (binding.returnKind) {
  case ModuleCppTypeKind::Void:
    ffi_call(&binding.cif, FFI_FN(binding.symbol), nullptr, argPointers.data());
    return 1;
  case ModuleCppTypeKind::Int: {
    int32_t result = 0;
    ffi_call(&binding.cif, FFI_FN(binding.symbol), &result, argPointers.data());
    if (out_value != nullptr) {
      out_value->kind = NEURON_MODULECPP_KIND_INT;
      out_value->int_value = result;
    }
    return 1;
  }
  case ModuleCppTypeKind::Float: {
    float result = 0.0f;
    ffi_call(&binding.cif, FFI_FN(binding.symbol), &result, argPointers.data());
    if (out_value != nullptr) {
      out_value->kind = NEURON_MODULECPP_KIND_FLOAT;
      out_value->float_value = result;
    }
    return 1;
  }
  case ModuleCppTypeKind::Double: {
    double result = 0.0;
    ffi_call(&binding.cif, FFI_FN(binding.symbol), &result, argPointers.data());
    if (out_value != nullptr) {
      out_value->kind = NEURON_MODULECPP_KIND_DOUBLE;
      out_value->float_value = result;
    }
    return 1;
  }
  case ModuleCppTypeKind::Bool: {
    std::uint8_t result = 0;
    ffi_call(&binding.cif, FFI_FN(binding.symbol), &result, argPointers.data());
    if (out_value != nullptr) {
      out_value->kind = NEURON_MODULECPP_KIND_BOOL;
      out_value->int_value = result != 0 ? 1 : 0;
    }
    return 1;
  }
  case ModuleCppTypeKind::String: {
    const char *result = nullptr;
    ffi_call(&binding.cif, FFI_FN(binding.symbol), &result, argPointers.data());
    g_lastReturnedString = result == nullptr ? "" : result;
    if (out_value != nullptr) {
      out_value->kind = NEURON_MODULECPP_KIND_STRING;
      out_value->string_value = g_lastReturnedString.c_str();
    }
    return 1;
  }
  }

  setModuleCppError(std::string("unsupported modulecpp return kind for ") +
                    call_target);
  return 0;
}

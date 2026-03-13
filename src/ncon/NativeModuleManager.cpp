#include "neuronc/ncon/NativeModuleManager.h"
#include "neuronc/ncon/Sha256.h"

#include "neuronc/ncon/Sandbox.h"
#include "neuron_platform.h"

#include <ffi.h>

#include <cstdint>
#include <fstream>
#include <cstdlib>

namespace neuron::ncon {

namespace fs = std::filesystem;

namespace {

enum class NativeArgKind { IntLike, Float, Double, Bool, String };
enum class NativeReturnKind { Void, Int, Float, Double, Bool, String };

bool readFileBytes(const fs::path &path, std::vector<uint8_t> *outBytes) {
  if (outBytes == nullptr) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  in.seekg(0, std::ios::beg);
  if (size < 0) {
    return false;
  }
  outBytes->resize(static_cast<size_t>(size));
  if (!outBytes->empty()) {
    in.read(reinterpret_cast<char *>(outBytes->data()),
            static_cast<std::streamsize>(outBytes->size()));
    if (in.gcount() != static_cast<std::streamsize>(outBytes->size())) {
      return false;
    }
  }
  return true;
}

bool writeFileBytes(const fs::path &path, const std::vector<uint8_t> &bytes) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    return false;
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  if (!bytes.empty()) {
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }
  return out.good();
}

std::string hashBytes(const std::vector<uint8_t> &bytes) {
  return sha256Hex(bytes);
}

std::string currentHostPlatform() {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
  return "windows_x64";
#elif defined(__linux__) && defined(__x86_64__)
  return "linux_x64";
#elif defined(__APPLE__) && defined(__aarch64__)
  return "macos_arm64";
#else
  return "unsupported";
#endif
}

fs::path nativeCacheRoot() {
#ifdef _WIN32
  const char *localAppData = std::getenv("LOCALAPPDATA");
  if (localAppData != nullptr && *localAppData != '\0') {
    return fs::path(localAppData) / "Neuron" / "ncon" / "native";
  }
#else
  const char *xdgCache = std::getenv("XDG_CACHE_HOME");
  if (xdgCache != nullptr && *xdgCache != '\0') {
    return fs::path(xdgCache) / "Neuron" / "ncon" / "native";
  }
  const char *home = std::getenv("HOME");
  if (home != nullptr && *home != '\0') {
    return fs::path(home) / ".cache" / "Neuron" / "ncon" / "native";
  }
#endif
  return fs::temp_directory_path() / "Neuron" / "ncon" / "native";
}

std::string toString(const VMValue &value) {
  if (const auto *v = std::get_if<std::string>(&value.data)) {
    return *v;
  }
  if (const auto *v = std::get_if<int64_t>(&value.data)) {
    return std::to_string(*v);
  }
  if (const auto *v = std::get_if<double>(&value.data)) {
    return std::to_string(*v);
  }
  return "";
}

int32_t toInt32(const VMValue &value) {
  if (const auto *v = std::get_if<int64_t>(&value.data)) {
    return static_cast<int32_t>(*v);
  }
  if (const auto *v = std::get_if<double>(&value.data)) {
    return static_cast<int32_t>(*v);
  }
  return 0;
}

float toFloat32(const VMValue &value) {
  if (const auto *v = std::get_if<double>(&value.data)) {
    return static_cast<float>(*v);
  }
  if (const auto *v = std::get_if<int64_t>(&value.data)) {
    return static_cast<float>(*v);
  }
  return 0.0f;
}

double toFloat64(const VMValue &value) {
  if (const auto *v = std::get_if<double>(&value.data)) {
    return *v;
  }
  if (const auto *v = std::get_if<int64_t>(&value.data)) {
    return static_cast<double>(*v);
  }
  return 0.0;
}

NativeArgKind argKindFromType(const std::string &typeName,
                              std::string *outError = nullptr) {
  if (typeName == "int") {
    return NativeArgKind::IntLike;
  }
  if (typeName == "float") {
    return NativeArgKind::Float;
  }
  if (typeName == "double") {
    return NativeArgKind::Double;
  }
  if (typeName == "bool") {
    return NativeArgKind::Bool;
  }
  if (typeName == "string") {
    return NativeArgKind::String;
  }
  if (outError != nullptr) {
    *outError = "unsupported native argument type: " + typeName;
  }
  return NativeArgKind::IntLike;
}

bool returnKindFromType(const std::string &typeName, NativeReturnKind *outKind,
                        std::string *outError = nullptr) {
  if (outKind == nullptr) {
    return false;
  }
  if (typeName == "void") {
    *outKind = NativeReturnKind::Void;
    return true;
  }
  if (typeName == "int") {
    *outKind = NativeReturnKind::Int;
    return true;
  }
  if (typeName == "float") {
    *outKind = NativeReturnKind::Float;
    return true;
  }
  if (typeName == "double") {
    *outKind = NativeReturnKind::Double;
    return true;
  }
  if (typeName == "bool") {
    *outKind = NativeReturnKind::Bool;
    return true;
  }
  if (typeName == "string") {
    *outKind = NativeReturnKind::String;
    return true;
  }
  if (outError != nullptr) {
    *outError = "unsupported native return type: " + typeName;
  }
  return false;
}

ffi_type *ffiTypeFor(NativeArgKind kind) {
  switch (kind) {
  case NativeArgKind::IntLike:
    return &ffi_type_sint32;
  case NativeArgKind::Float:
    return &ffi_type_float;
  case NativeArgKind::Double:
    return &ffi_type_double;
  case NativeArgKind::Bool:
    return &ffi_type_uint8;
  case NativeArgKind::String:
    return &ffi_type_pointer;
  }
  return &ffi_type_void;
}

ffi_type *ffiTypeFor(NativeReturnKind kind) {
  switch (kind) {
  case NativeReturnKind::Void:
    return &ffi_type_void;
  case NativeReturnKind::Int:
    return &ffi_type_sint32;
  case NativeReturnKind::Float:
    return &ffi_type_float;
  case NativeReturnKind::Double:
    return &ffi_type_double;
  case NativeReturnKind::Bool:
    return &ffi_type_uint8;
  case NativeReturnKind::String:
    return &ffi_type_pointer;
  }
  return &ffi_type_void;
}

struct NativeArgumentStorage {
  int32_t intValue = 0;
  float floatValue = 0.0f;
  double doubleValue = 0.0;
  std::uint8_t boolValue = 0;
  const char *stringValue = nullptr;
  std::string stringStorage;
};

bool invokeBySignature(void *symbol, const std::vector<NativeArgKind> &argKinds,
                       NativeReturnKind returnKind,
                       const std::vector<VMValue> &args, VMValue *outValue,
                       std::string *outError) {
  if (args.size() != argKinds.size()) {
    if (outError != nullptr) {
      *outError = "native module argument count mismatch";
    }
    return false;
  }

  std::vector<NativeArgumentStorage> storage(args.size());
  std::vector<void *> argPointers(args.size(), nullptr);
  for (size_t i = 0; i < args.size(); ++i) {
    NativeArgumentStorage &slot = storage[i];
    switch (argKinds[i]) {
    case NativeArgKind::IntLike:
      slot.intValue = toInt32(args[i]);
      argPointers[i] = &slot.intValue;
      break;
    case NativeArgKind::Float:
      slot.floatValue = toFloat32(args[i]);
      argPointers[i] = &slot.floatValue;
      break;
    case NativeArgKind::Double:
      slot.doubleValue = toFloat64(args[i]);
      argPointers[i] = &slot.doubleValue;
      break;
    case NativeArgKind::Bool:
      slot.boolValue = toInt32(args[i]) != 0 ? 1u : 0u;
      argPointers[i] = &slot.boolValue;
      break;
    case NativeArgKind::String:
      slot.stringStorage = toString(args[i]);
      slot.stringValue = slot.stringStorage.c_str();
      argPointers[i] = &slot.stringValue;
      break;
    }
  }

  if (outValue != nullptr) {
    outValue->data = std::monostate{};
  }

  std::vector<ffi_type *> ffiArgTypes;
  ffiArgTypes.reserve(argKinds.size());
  for (NativeArgKind kind : argKinds) {
    ffiArgTypes.push_back(ffiTypeFor(kind));
  }

  ffi_cif cif{};
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
                   static_cast<unsigned int>(ffiArgTypes.size()),
                   ffiTypeFor(returnKind), ffiArgTypes.data()) != FFI_OK) {
    if (outError != nullptr) {
      *outError = "failed to prepare native ffi dispatch";
    }
    return false;
  }

  switch (returnKind) {
  case NativeReturnKind::Void:
    ffi_call(&cif, FFI_FN(symbol), nullptr, argPointers.data());
    return true;
  case NativeReturnKind::Int: {
    int32_t result = 0;
    ffi_call(&cif, FFI_FN(symbol), &result, argPointers.data());
    if (outValue != nullptr) {
      outValue->data = static_cast<int64_t>(result);
    }
    return true;
  }
  case NativeReturnKind::Float: {
    float result = 0.0f;
    ffi_call(&cif, FFI_FN(symbol), &result, argPointers.data());
    if (outValue != nullptr) {
      outValue->data = static_cast<double>(result);
    }
    return true;
  }
  case NativeReturnKind::Double: {
    double result = 0.0;
    ffi_call(&cif, FFI_FN(symbol), &result, argPointers.data());
    if (outValue != nullptr) {
      outValue->data = result;
    }
    return true;
  }
  case NativeReturnKind::Bool: {
    std::uint8_t result = 0;
    ffi_call(&cif, FFI_FN(symbol), &result, argPointers.data());
    if (outValue != nullptr) {
      outValue->data = static_cast<int64_t>(result != 0 ? 1 : 0);
    }
    return true;
  }
  case NativeReturnKind::String: {
    const char *result = nullptr;
    ffi_call(&cif, FFI_FN(symbol), &result, argPointers.data());
    if (outValue != nullptr) {
      outValue->data = result == nullptr ? std::string() : std::string(result);
    }
    return true;
  }
  }

  if (outError != nullptr) {
    *outError = "unsupported native return kind";
  }
  return false;
}

} // namespace

struct NativeModuleManager::LoadedLibrary {
  std::string moduleName;
  fs::path path;
  NeuronPlatformLibraryHandle handle = nullptr;
};

struct NativeModuleManager::LoadedExport {
  void *symbol = nullptr;
  NativeReturnKind returnKind = NativeReturnKind::Void;
  std::vector<NativeArgKind> argKinds;
};

NativeModuleManager::NativeModuleManager() = default;

NativeModuleManager::~NativeModuleManager() { unload(); }

bool NativeModuleManager::load(const ContainerData &container,
                               const SandboxContext *sandbox,
                               std::string *outError) {
  unload();

  ManifestData manifest;
  if (!parseManifestData(container.manifestJson, &manifest, outError)) {
    return false;
  }
  if (!manifest.nativeEnabled || manifest.nativeModules.empty()) {
    return true;
  }

  if (sandbox != nullptr && !sandbox->nativeCacheDirectory.empty()) {
    m_cacheRoot = sandbox->nativeCacheDirectory;
  } else {
    m_cacheRoot = nativeCacheRoot();
  }

  for (const auto &module : manifest.nativeModules) {
    if (!loadModule(container, module, outError)) {
      unload();
      return false;
    }
  }
  return true;
}

void NativeModuleManager::unload() {
  m_exports.clear();
  for (auto &library : m_libraries) {
    if (library.handle == nullptr) {
      continue;
    }
    neuron_platform_close_library(library.handle);
    library.handle = nullptr;
  }
  m_libraries.clear();
}

bool NativeModuleManager::hasCallTarget(const std::string &callTarget) const {
  return m_exports.find(callTarget) != m_exports.end();
}

bool NativeModuleManager::invoke(const std::string &callTarget,
                                 const std::vector<VMValue> &args,
                                 VMValue *outValue,
                                 std::string *outError) const {
  auto exportIt = m_exports.find(callTarget);
  if (exportIt == m_exports.end()) {
    if (outError != nullptr) {
      *outError = "native module call target not found: " + callTarget;
    }
    return false;
  }
  if (args.size() != exportIt->second.argKinds.size()) {
    if (outError != nullptr) {
      *outError = "native module argument count mismatch for " + callTarget;
    }
    return false;
  }
  return invokeBySignature(exportIt->second.symbol, exportIt->second.argKinds,
                           exportIt->second.returnKind, args, outValue,
                           outError);
}

bool NativeModuleManager::loadModule(const ContainerData &container,
                                     const NativeModuleManifestInfo &module,
                                     std::string *outError) {
  const std::string hostPlatform = currentHostPlatform();
  if (hostPlatform == "unsupported") {
    if (outError != nullptr) {
      *outError = "modulecpp '" + module.name + "' unsupported on this host";
    }
    return false;
  }

  const NativeArtifactInfo *selectedArtifact = nullptr;
  for (const auto &artifact : module.artifacts) {
    if (artifact.platform == hostPlatform) {
      selectedArtifact = &artifact;
      break;
    }
  }
  if (selectedArtifact == nullptr) {
    if (outError != nullptr) {
      *outError =
          "modulecpp '" + module.name + "' unsupported on " + hostPlatform;
    }
    return false;
  }

  const ResourceIndexEntry *resourceEntry = nullptr;
  for (const auto &resource : container.resources) {
    if (resource.id == selectedArtifact->resourceId) {
      resourceEntry = &resource;
      break;
    }
  }
  if (resourceEntry == nullptr) {
    if (outError != nullptr) {
      *outError = "native module resource not found: " +
                  selectedArtifact->resourceId;
    }
    return false;
  }
  if (resourceEntry->blobOffset + resourceEntry->size >
      container.resourcesBlob.size()) {
    if (outError != nullptr) {
      *outError = "native module resource out of range: " +
                  selectedArtifact->resourceId;
    }
    return false;
  }

  std::vector<uint8_t> bytes(
      container.resourcesBlob.begin() +
          static_cast<std::ptrdiff_t>(resourceEntry->blobOffset),
      container.resourcesBlob.begin() +
          static_cast<std::ptrdiff_t>(resourceEntry->blobOffset +
                                      resourceEntry->size));
  if (crc32(bytes) != selectedArtifact->crc32 ||
      hashBytes(bytes) != selectedArtifact->sha256) {
    if (outError != nullptr) {
      *outError = "native module integrity check failed for " + module.name;
    }
    return false;
  }

  const fs::path cachePath =
      m_cacheRoot / selectedArtifact->sha256 / selectedArtifact->fileName;
  std::vector<uint8_t> cachedBytes;
  if (!fs::exists(cachePath) || !readFileBytes(cachePath, &cachedBytes) ||
      hashBytes(cachedBytes) != selectedArtifact->sha256) {
    if (!writeFileBytes(cachePath, bytes)) {
      if (outError != nullptr) {
        *outError = "failed to materialize native module cache file: " +
                    cachePath.string();
      }
      return false;
    }
  }

  LoadedLibrary library;
  library.moduleName = module.name;
  library.path = cachePath;
  library.handle = neuron_platform_open_library(cachePath.string().c_str());
  if (library.handle == nullptr) {
    if (outError != nullptr) {
      const char *platformError = neuron_platform_last_error();
      *outError = "failed to load native module library: " + cachePath.string();
      if (platformError != nullptr && *platformError != '\0') {
        *outError += " (";
        *outError += platformError;
        *outError += ")";
      }
    }
    return false;
  }

  auto closeLocalLibrary = [&]() {
    if (library.handle != nullptr) {
      neuron_platform_close_library(library.handle);
      library.handle = nullptr;
    }
  };

  for (const auto &exportInfo : module.exports) {
    void *symbolPtr =
        neuron_platform_load_symbol(library.handle, exportInfo.symbol.c_str());
    if (symbolPtr == nullptr) {
      if (outError != nullptr) {
        *outError = "modulecpp '" + module.name + "' missing symbol '" +
                    exportInfo.symbol + "'";
        const char *platformError = neuron_platform_last_error();
        if (platformError != nullptr && *platformError != '\0') {
          *outError += " (";
          *outError += platformError;
          *outError += ")";
        }
      }
      closeLocalLibrary();
      return false;
    }

    LoadedExport loadedExport;
    loadedExport.symbol = symbolPtr;
    if (!returnKindFromType(exportInfo.returnType, &loadedExport.returnKind,
                            outError)) {
      closeLocalLibrary();
      return false;
    }
    for (const auto &paramType : exportInfo.parameterTypes) {
      loadedExport.argKinds.push_back(argKindFromType(paramType, outError));
    }
    const std::string exportKey = module.name + "." + exportInfo.name;
    m_exports[exportKey] = std::move(loadedExport);
  }

  m_libraries.push_back(std::move(library));
  return true;
}

} // namespace neuron::ncon

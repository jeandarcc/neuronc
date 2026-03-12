#include "VMInternal.h"

namespace neuron::ncon::detail {

std::string canonicalTypeKey(const Program &program, uint32_t typeId,
                             std::unordered_map<uint32_t, std::string> *cache) {
  if (typeId == kInvalidIndex || typeId >= program.types.size()) {
    return "<invalid>";
  }
  if (cache != nullptr) {
    auto existing = cache->find(typeId);
    if (existing != cache->end()) {
      return existing->second;
    }
  }

  const TypeRecord &type = program.types[typeId];
  auto recurse = [&](uint32_t nestedTypeId) {
    return canonicalTypeKey(program, nestedTypeId, cache);
  };

  std::ostringstream out;
  switch (type.kind) {
  case TypeKind::Void:
    out << "void";
    break;
  case TypeKind::Int:
    out << "int";
    break;
  case TypeKind::Float:
    out << "float";
    break;
  case TypeKind::Double:
    out << "double";
    break;
  case TypeKind::Bool:
    out << "bool";
    break;
  case TypeKind::String:
    out << "string";
    break;
  case TypeKind::Char:
    out << "char";
    break;
  case TypeKind::Nullable:
    if (!type.genericTypeIds.empty()) {
      out << "maybe(" << recurse(type.genericTypeIds.front()) << ")";
    } else {
      out << "maybe";
    }
    break;
  case TypeKind::Pointer:
    out << "ptr(" << recurse(type.pointeeTypeId) << ")";
    break;
  case TypeKind::Array:
    out << "array(" << recurse(type.pointeeTypeId) << ")";
    break;
  case TypeKind::Tensor:
    out << "tensor(" << recurse(type.pointeeTypeId) << ")";
    break;
  case TypeKind::Class:
    out << "class:";
    if (type.nameStringId != kInvalidIndex && type.nameStringId < program.strings.size()) {
      out << program.strings[type.nameStringId];
    }
    out << '{';
    for (size_t i = 0; i < type.fieldTypeIds.size(); ++i) {
      if (i != 0) {
        out << ',';
      }
      if (i < type.fieldNameStringIds.size() &&
          type.fieldNameStringIds[i] < program.strings.size()) {
        out << program.strings[type.fieldNameStringIds[i]];
      }
      out << ':' << recurse(type.fieldTypeIds[i]);
    }
    out << '}';
    break;
  case TypeKind::Dynamic:
    out << "dynamic";
    break;
  case TypeKind::Dictionary:
    out << "dictionary";
    break;
  case TypeKind::Generic:
    out << "generic";
    break;
  case TypeKind::Enum:
    out << "enum";
    break;
  case TypeKind::Unknown:
  case TypeKind::Auto:
  case TypeKind::Error:
  case TypeKind::Method:
  case TypeKind::Module:
    out << "kind:" << static_cast<int>(type.kind);
    break;
  }

  const std::string key = out.str();
  if (cache != nullptr) {
    (*cache)[typeId] = key;
  }
  return key;
}

bool manifestsCompatible(const ContainerData &before, const ContainerData &after,
                         std::string *outReason) {
  ManifestData beforeManifest;
  ManifestData afterManifest;
  std::string error;
  const bool beforeParsed =
      parseManifestData(before.manifestJson, &beforeManifest, &error);
  const bool afterParsed = parseManifestData(after.manifestJson, &afterManifest, &error);
  if (!beforeParsed || !afterParsed) {
    return true;
  }

  if (beforeManifest.nativeEnabled != afterManifest.nativeEnabled) {
    if (outReason != nullptr) {
      *outReason = "native module enablement changed";
    }
    return false;
  }
  if (beforeManifest.permissions.fsRead != afterManifest.permissions.fsRead ||
      beforeManifest.permissions.fsWrite != afterManifest.permissions.fsWrite ||
      beforeManifest.permissions.network != afterManifest.permissions.network ||
      beforeManifest.permissions.processSpawnAllowed !=
          afterManifest.permissions.processSpawnAllowed) {
    if (outReason != nullptr) {
      *outReason = "runtime permissions changed";
    }
    return false;
  }
  if (beforeManifest.resources.size() != afterManifest.resources.size()) {
    if (outReason != nullptr) {
      *outReason = "resource set changed";
    }
    return false;
  }
  for (size_t i = 0; i < beforeManifest.resources.size(); ++i) {
    const ResourceInfo &beforeResource = beforeManifest.resources[i];
    const ResourceInfo &afterResource = afterManifest.resources[i];
    if (beforeResource.id != afterResource.id ||
        beforeResource.crc32 != afterResource.crc32 ||
        beforeResource.size != afterResource.size) {
      if (outReason != nullptr) {
        *outReason = "resource payload changed";
      }
      return false;
    }
  }
  if (beforeManifest.nativeModules.size() != afterManifest.nativeModules.size()) {
    if (outReason != nullptr) {
      *outReason = "native module set changed";
    }
    return false;
  }
  for (size_t i = 0; i < beforeManifest.nativeModules.size(); ++i) {
    const NativeModuleManifestInfo &beforeModule = beforeManifest.nativeModules[i];
    const NativeModuleManifestInfo &afterModule = afterManifest.nativeModules[i];
    if (beforeModule.name != afterModule.name || beforeModule.abi != afterModule.abi ||
        beforeModule.exports.size() != afterModule.exports.size() ||
        beforeModule.artifacts.size() != afterModule.artifacts.size()) {
      if (outReason != nullptr) {
        *outReason = "native module manifest changed";
      }
      return false;
    }
    for (size_t exportIndex = 0; exportIndex < beforeModule.exports.size();
         ++exportIndex) {
      const NativeExportInfo &beforeExport = beforeModule.exports[exportIndex];
      const NativeExportInfo &afterExport = afterModule.exports[exportIndex];
      if (beforeExport.name != afterExport.name ||
          beforeExport.symbol != afterExport.symbol ||
          beforeExport.parameterTypes != afterExport.parameterTypes ||
          beforeExport.returnType != afterExport.returnType) {
        if (outReason != nullptr) {
          *outReason = "native export signature changed";
        }
        return false;
      }
    }
    for (size_t artifactIndex = 0; artifactIndex < beforeModule.artifacts.size();
         ++artifactIndex) {
      const NativeArtifactInfo &beforeArtifact =
          beforeModule.artifacts[artifactIndex];
      const NativeArtifactInfo &afterArtifact =
          afterModule.artifacts[artifactIndex];
      if (beforeArtifact.platform != afterArtifact.platform ||
          beforeArtifact.sha256 != afterArtifact.sha256 ||
          beforeArtifact.resourceId != afterArtifact.resourceId) {
        if (outReason != nullptr) {
          *outReason = "native artifact payload changed";
        }
        return false;
      }
    }
  }
  return true;
}

bool analyzeHotReloadCompatibility(const ContainerData &before,
                                   const ContainerData &after,
                                   HotReloadAnalysis *outAnalysis) {
  if (outAnalysis == nullptr) {
    return false;
  }
  *outAnalysis = HotReloadAnalysis{};

  if (!manifestsCompatible(before, after, &outAnalysis->reason)) {
    return true;
  }

  std::unordered_map<uint32_t, std::string> beforeTypeKeys;
  std::unordered_map<uint32_t, std::string> afterTypeKeys;
  std::unordered_map<std::string, uint32_t> afterTypeByKey;
  for (uint32_t i = 0; i < after.program.types.size(); ++i) {
    afterTypeByKey[canonicalTypeKey(after.program, i, &afterTypeKeys)] = i;
  }
  for (uint32_t i = 0; i < before.program.types.size(); ++i) {
    const std::string key = canonicalTypeKey(before.program, i, &beforeTypeKeys);
    auto afterIt = afterTypeByKey.find(key);
    if (afterIt == afterTypeByKey.end()) {
      outAnalysis->reason = "type layout changed";
      return true;
    }
    outAnalysis->typeRemap[i] = afterIt->second;
  }

  if (before.program.globals.size() != after.program.globals.size()) {
    outAnalysis->reason = "global set changed";
    return true;
  }
  for (size_t i = 0; i < before.program.globals.size(); ++i) {
    const GlobalRecord &beforeGlobal = before.program.globals[i];
    const GlobalRecord &afterGlobal = after.program.globals[i];
    const std::string beforeName =
        beforeGlobal.nameStringId < before.program.strings.size()
            ? before.program.strings[beforeGlobal.nameStringId]
            : std::string();
    const std::string afterName =
        afterGlobal.nameStringId < after.program.strings.size()
            ? after.program.strings[afterGlobal.nameStringId]
            : std::string();
    if (beforeName != afterName ||
        canonicalTypeKey(before.program, beforeGlobal.typeId, &beforeTypeKeys) !=
            canonicalTypeKey(after.program, afterGlobal.typeId, &afterTypeKeys)) {
      outAnalysis->reason = "global layout changed";
      return true;
    }
  }

  auto functionSignature = [](const Program &program, const FunctionRecord &fn,
                              std::unordered_map<uint32_t, std::string> *typeKeys) {
    std::ostringstream out;
    out << canonicalTypeKey(program, fn.returnTypeId, typeKeys) << '(';
    for (size_t i = 0; i < fn.argTypeIds.size(); ++i) {
      if (i != 0) {
        out << ',';
      }
      out << canonicalTypeKey(program, fn.argTypeIds[i], typeKeys);
    }
    out << ')';
    return out.str();
  };

  std::unordered_map<std::string, std::string> afterFunctionSignatures;
  for (const auto &fn : after.program.functions) {
    const std::string name =
        fn.nameStringId < after.program.strings.size()
            ? after.program.strings[fn.nameStringId]
            : std::string();
    afterFunctionSignatures[name] = functionSignature(after.program, fn, &afterTypeKeys);
  }
  for (const auto &fn : before.program.functions) {
    const std::string name =
        fn.nameStringId < before.program.strings.size()
            ? before.program.strings[fn.nameStringId]
            : std::string();
    auto afterIt = afterFunctionSignatures.find(name);
    if (afterIt == afterFunctionSignatures.end()) {
      outAnalysis->reason = "function removed during hot reload";
      return true;
    }
    if (afterIt->second != functionSignature(before.program, fn, &beforeTypeKeys)) {
      outAnalysis->reason = "function signature changed";
      return true;
    }
  }

  bool hasHotReload = false;
  for (const auto &fn : after.program.functions) {
    if (fn.nameStringId < after.program.strings.size() &&
        after.program.strings[fn.nameStringId] == "HotReload") {
      hasHotReload = true;
      break;
    }
  }
  outAnalysis->hookFunction = hasHotReload ? "HotReload" : "Init";
  outAnalysis->compatible = true;
  return true;
}

} // namespace neuron::ncon::detail


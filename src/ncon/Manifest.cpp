#include "neuronc/ncon/Manifest.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace neuron::ncon {

namespace {

std::string escapeJson(const std::string &text) {
  std::ostringstream out;
  for (char ch : text) {
    switch (ch) {
    case '\\':
      out << "\\\\";
      break;
    case '"':
      out << "\\\"";
      break;
    case '\n':
      out << "\\n";
      break;
    case '\r':
      out << "\\r";
      break;
    case '\t':
      out << "\\t";
      break;
    default:
      out << ch;
      break;
    }
  }
  return out.str();
}

const char *networkPolicyName(NconNetworkPolicy policy) {
  switch (policy) {
  case NconNetworkPolicy::Deny:
    return "deny";
  case NconNetworkPolicy::Allow:
    return "allow";
  }
  return "deny";
}

void appendStringArray(std::ostringstream &out,
                       const std::vector<std::string> &values) {
  out << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << "\"" << escapeJson(values[i]) << "\"";
  }
  out << "]";
}

std::string trimCopy(std::string text) {
  auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
  text.erase(text.begin(),
             std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

bool extractJsonArray(const std::string &text, const std::string &key,
                      std::vector<std::string> *outValues) {
  if (outValues == nullptr) {
    return false;
  }

  const std::string needle = "\"" + key + "\"";
  const std::size_t keyPos = text.find(needle);
  if (keyPos == std::string::npos) {
    return false;
  }
  const std::size_t bracketOpen = text.find('[', keyPos);
  const std::size_t bracketClose = text.find(']', bracketOpen);
  if (bracketOpen == std::string::npos || bracketClose == std::string::npos) {
    return false;
  }

  outValues->clear();
  std::string body = text.substr(bracketOpen + 1, bracketClose - bracketOpen - 1);
  std::size_t cursor = 0;
  while (cursor < body.size()) {
    while (cursor < body.size() &&
           (std::isspace(static_cast<unsigned char>(body[cursor])) ||
            body[cursor] == ',')) {
      ++cursor;
    }
    if (cursor >= body.size()) {
      break;
    }
    if (body[cursor] != '"') {
      return false;
    }
    ++cursor;
    std::string value;
    while (cursor < body.size()) {
      const char ch = body[cursor++];
      if (ch == '\\') {
        if (cursor >= body.size()) {
          return false;
        }
        const char escaped = body[cursor++];
        switch (escaped) {
        case '\\':
        case '"':
        case '/':
          value.push_back(escaped);
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          value.push_back(escaped);
          break;
        }
        continue;
      }
      if (ch == '"') {
        break;
      }
      value.push_back(ch);
    }
    outValues->push_back(value);
  }

  return true;
}

bool extractJsonBlock(const std::string &text, const std::string &key,
                      char openChar, char closeChar, std::string *outBlock) {
  if (outBlock == nullptr) {
    return false;
  }
  const std::string needle = "\"" + key + "\"";
  const std::size_t keyPos = text.find(needle);
  if (keyPos == std::string::npos) {
    return false;
  }

  std::size_t cursor = text.find(':', keyPos);
  if (cursor == std::string::npos) {
    return false;
  }
  cursor = text.find(openChar, cursor);
  if (cursor == std::string::npos) {
    return false;
  }

  bool inString = false;
  bool escaped = false;
  int depth = 0;
  const std::size_t begin = cursor;
  for (; cursor < text.size(); ++cursor) {
    const char ch = text[cursor];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (ch == '\\' && inString) {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      inString = !inString;
      continue;
    }
    if (inString) {
      continue;
    }
    if (ch == openChar) {
      ++depth;
    } else if (ch == closeChar) {
      --depth;
      if (depth == 0) {
        *outBlock = text.substr(begin, cursor - begin + 1);
        return true;
      }
    }
  }
  return false;
}

bool extractJsonString(const std::string &text, const std::string &key,
                       std::string *outValue) {
  if (outValue == nullptr) {
    return false;
  }
  const std::string needle = "\"" + key + "\"";
  const std::size_t keyPos = text.find(needle);
  if (keyPos == std::string::npos) {
    return false;
  }
  const std::size_t quoteBegin = text.find('"', text.find(':', keyPos));
  if (quoteBegin == std::string::npos) {
    return false;
  }
  std::size_t cursor = quoteBegin + 1;
  std::string value;
  while (cursor < text.size()) {
    const char ch = text[cursor++];
    if (ch == '\\') {
      if (cursor >= text.size()) {
        return false;
      }
      const char escaped = text[cursor++];
      switch (escaped) {
      case '\\':
      case '"':
      case '/':
        value.push_back(escaped);
        break;
      case 'n':
        value.push_back('\n');
        break;
      case 'r':
        value.push_back('\r');
        break;
      case 't':
        value.push_back('\t');
        break;
      default:
        value.push_back(escaped);
        break;
      }
      continue;
    }
    if (ch == '"') {
      *outValue = value;
      return true;
    }
    value.push_back(ch);
  }
  return false;
}

bool extractJsonBool(const std::string &text, const std::string &key,
                     bool *outValue) {
  if (outValue == nullptr) {
    return false;
  }
  const std::string needle = "\"" + key + "\"";
  const std::size_t keyPos = text.find(needle);
  if (keyPos == std::string::npos) {
    return false;
  }
  std::size_t cursor = text.find(':', keyPos);
  if (cursor == std::string::npos) {
    return false;
  }
  ++cursor;
  while (cursor < text.size() &&
         std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
    ++cursor;
  }
  if (text.compare(cursor, 4, "true") == 0) {
    *outValue = true;
    return true;
  }
  if (text.compare(cursor, 5, "false") == 0) {
    *outValue = false;
    return true;
  }
  return false;
}

bool extractJsonUInt64(const std::string &text, const std::string &key,
                       uint64_t *outValue) {
  if (outValue == nullptr) {
    return false;
  }
  const std::string needle = "\"" + key + "\"";
  const std::size_t keyPos = text.find(needle);
  if (keyPos == std::string::npos) {
    return false;
  }
  std::size_t cursor = text.find(':', keyPos);
  if (cursor == std::string::npos) {
    return false;
  }
  ++cursor;
  while (cursor < text.size() &&
         std::isspace(static_cast<unsigned char>(text[cursor])) != 0) {
    ++cursor;
  }
  std::size_t end = cursor;
  while (end < text.size() &&
         std::isdigit(static_cast<unsigned char>(text[end])) != 0) {
    ++end;
  }
  if (end == cursor) {
    return false;
  }
  try {
    *outValue = static_cast<uint64_t>(
        std::stoull(text.substr(cursor, end - cursor)));
    return true;
  } catch (...) {
    return false;
  }
}

bool extractJsonUInt32(const std::string &text, const std::string &key,
                       uint32_t *outValue) {
  uint64_t value = 0;
  if (!extractJsonUInt64(text, key, &value)) {
    return false;
  }
  *outValue = static_cast<uint32_t>(value);
  return true;
}

bool splitJsonObjectArray(const std::string &arrayText,
                          std::vector<std::string> *outItems) {
  if (outItems == nullptr) {
    return false;
  }
  outItems->clear();

  const std::string trimmed = trimCopy(arrayText);
  if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
    return false;
  }

  bool inString = false;
  bool escaped = false;
  int depth = 0;
  std::size_t itemBegin = std::string::npos;
  for (std::size_t i = 1; i + 1 < trimmed.size(); ++i) {
    const char ch = trimmed[i];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (ch == '\\' && inString) {
      escaped = true;
      continue;
    }
    if (ch == '"') {
      inString = !inString;
      continue;
    }
    if (inString) {
      continue;
    }
    if (ch == '{') {
      if (depth == 0) {
        itemBegin = i;
      }
      ++depth;
    } else if (ch == '}') {
      --depth;
      if (depth == 0 && itemBegin != std::string::npos) {
        outItems->push_back(trimmed.substr(itemBegin, i - itemBegin + 1));
        itemBegin = std::string::npos;
      }
    }
  }
  return true;
}

bool parseJsonStringArrayBody(const std::string &arrayText,
                              std::vector<std::string> *outValues) {
  if (outValues == nullptr) {
    return false;
  }
  outValues->clear();
  const std::string trimmed = trimCopy(arrayText);
  if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
    return false;
  }

  std::size_t cursor = 1;
  while (cursor + 1 < trimmed.size()) {
    while (cursor + 1 < trimmed.size() &&
           (std::isspace(static_cast<unsigned char>(trimmed[cursor])) != 0 ||
            trimmed[cursor] == ',')) {
      ++cursor;
    }
    if (cursor + 1 >= trimmed.size()) {
      break;
    }
    if (trimmed[cursor] != '"') {
      return false;
    }
    ++cursor;
    std::string value;
    bool escaped = false;
    while (cursor + 1 < trimmed.size()) {
      const char ch = trimmed[cursor++];
      if (escaped) {
        switch (ch) {
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          value.push_back(ch);
          break;
        }
        escaped = false;
        continue;
      }
      if (ch == '\\') {
        escaped = true;
        continue;
      }
      if (ch == '"') {
        break;
      }
      value.push_back(ch);
    }
    outValues->push_back(std::move(value));
  }
  return true;
}

} // namespace

std::string buildManifestJson(const ManifestData &manifest) {
  std::ostringstream out;
  out << "{\n";
  out << "  \"format\": \"ncon-v1\",\n";
  out << "  \"app\": {\n";
  out << "    \"name\": \"" << escapeJson(manifest.appName) << "\",\n";
  out << "    \"version\": \"" << escapeJson(manifest.appVersion) << "\",\n";
  out << "    \"entry_module\": \"" << escapeJson(manifest.entryModule)
      << "\",\n";
  out << "    \"entry_function\": \"" << escapeJson(manifest.entryFunction)
      << "\"\n";
  out << "  },\n";
  out << "  \"ir\": {\n";
  out << "    \"format\": \"ncon-ir-v1\",\n";
  out << "    \"instruction_set_version\": 1\n";
  out << "  },\n";
  out << "  \"runtime\": {\n";
  out << "    \"abi\": \"ncon-runtime-v1\",\n";
  out << "    \"tensor_profile\": \"" << escapeJson(manifest.tensorProfile)
      << "\",\n";
  out << "    \"tensor_autotune\": "
      << (manifest.tensorAutotune ? "true" : "false") << ",\n";
  out << "    \"hot_reload\": " << (manifest.hotReload ? "true" : "false")
      << ",\n";
  out << "    \"native\": {\n";
  out << "      \"enabled\": " << (manifest.nativeEnabled ? "true" : "false")
      << ",\n";
  out << "      \"modules\": [\n";
  for (size_t i = 0; i < manifest.nativeModules.size(); ++i) {
    const NativeModuleManifestInfo &module = manifest.nativeModules[i];
    out << "        {\n";
    out << "          \"name\": \"" << escapeJson(module.name) << "\",\n";
    out << "          \"abi\": \"" << escapeJson(module.abi) << "\",\n";
    out << "          \"exports\": [\n";
    for (size_t exportIndex = 0; exportIndex < module.exports.size();
         ++exportIndex) {
      const NativeExportInfo &entry = module.exports[exportIndex];
      out << "            {\n";
      out << "              \"name\": \"" << escapeJson(entry.name) << "\",\n";
      out << "              \"symbol\": \"" << escapeJson(entry.symbol)
          << "\",\n";
      out << "              \"params\": ";
      appendStringArray(out, entry.parameterTypes);
      out << ",\n";
      out << "              \"return\": \"" << escapeJson(entry.returnType)
          << "\"\n";
      out << "            }"
          << (exportIndex + 1 == module.exports.size() ? "\n" : ",\n");
    }
    out << "          ],\n";
    out << "          \"artifacts\": [\n";
    for (size_t artifactIndex = 0; artifactIndex < module.artifacts.size();
         ++artifactIndex) {
      const NativeArtifactInfo &artifact = module.artifacts[artifactIndex];
      out << "            {\n";
      out << "              \"platform\": \"" << escapeJson(artifact.platform)
          << "\",\n";
      out << "              \"resource_id\": \""
          << escapeJson(artifact.resourceId) << "\",\n";
      out << "              \"file_name\": \"" << escapeJson(artifact.fileName)
          << "\",\n";
      out << "              \"size\": " << artifact.size << ",\n";
      out << "              \"crc32\": " << artifact.crc32 << ",\n";
      out << "              \"sha256\": \"" << escapeJson(artifact.sha256)
          << "\"\n";
      out << "            }"
          << (artifactIndex + 1 == module.artifacts.size() ? "\n" : ",\n");
    }
    out << "          ]\n";
    out << "        }"
        << (i + 1 == manifest.nativeModules.size() ? "\n" : ",\n");
  }
  out << "      ]\n";
  out << "    }\n";
  out << "  },\n";
  out << "  \"permissions\": {\n";
  out << "    \"fs_read\": ";
  appendStringArray(out, manifest.permissions.fsRead);
  out << ",\n";
  out << "    \"fs_write\": ";
  appendStringArray(out, manifest.permissions.fsWrite);
  out << ",\n";
  out << "    \"network\": \"" << networkPolicyName(manifest.permissions.network)
      << "\",\n";
  out << "    \"process_spawn\": \""
      << (manifest.permissions.processSpawnAllowed ? "allow" : "deny") << "\"\n";
  out << "  },\n";
  out << "  \"resources\": [\n";
  for (size_t i = 0; i < manifest.resources.size(); ++i) {
    const ResourceInfo &resource = manifest.resources[i];
    out << "    {\n";
    out << "      \"id\": \"" << escapeJson(resource.id) << "\",\n";
    out << "      \"size\": " << resource.size << ",\n";
    out << "      \"crc32\": " << resource.crc32 << "\n";
    out << "    }";
    out << (i + 1 == manifest.resources.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"build\": {\n";
  out << "    \"source_hash\": \"" << escapeJson(manifest.sourceHash)
      << "\",\n";
  out << "    \"optimize\": \"" << escapeJson(manifest.optimize) << "\",\n";
  out << "    \"target_cpu\": \"" << escapeJson(manifest.targetCPU) << "\"\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

bool parseManifestPermissions(const std::string &manifestJson,
                              neuron::NconPermissionConfig *outPermissions,
                              std::string *outError) {
  if (outPermissions == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null manifest permissions output";
    }
    return false;
  }

  std::vector<std::string> fsRead;
  std::vector<std::string> fsWrite;
  std::string network;
  std::string processSpawn;
  if (!extractJsonArray(manifestJson, "fs_read", &fsRead) ||
      !extractJsonArray(manifestJson, "fs_write", &fsWrite) ||
      !extractJsonString(manifestJson, "network", &network) ||
      !extractJsonString(manifestJson, "process_spawn", &processSpawn)) {
    if (outError != nullptr) {
      *outError = "invalid or incomplete manifest permissions block";
    }
    return false;
  }

  outPermissions->fsRead = std::move(fsRead);
  outPermissions->fsWrite = std::move(fsWrite);
  outPermissions->network =
      trimCopy(network) == "allow" ? NconNetworkPolicy::Allow
                                    : NconNetworkPolicy::Deny;
  outPermissions->processSpawnAllowed = trimCopy(processSpawn) == "allow";
  return true;
}

bool parseManifestData(const std::string &manifestJson, ManifestData *outManifest,
                       std::string *outError) {
  if (outManifest == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null manifest output";
    }
    return false;
  }

  *outManifest = ManifestData{};
  if (!extractJsonString(manifestJson, "name", &outManifest->appName) ||
      !extractJsonString(manifestJson, "version", &outManifest->appVersion) ||
      !extractJsonString(manifestJson, "entry_module",
                         &outManifest->entryModule) ||
      !extractJsonString(manifestJson, "entry_function",
                         &outManifest->entryFunction) ||
      !extractJsonString(manifestJson, "source_hash", &outManifest->sourceHash) ||
      !extractJsonString(manifestJson, "optimize", &outManifest->optimize) ||
      !extractJsonString(manifestJson, "target_cpu", &outManifest->targetCPU) ||
      !extractJsonString(manifestJson, "tensor_profile",
                         &outManifest->tensorProfile) ||
      !extractJsonBool(manifestJson, "tensor_autotune",
                       &outManifest->tensorAutotune) ||
      !extractJsonBool(manifestJson, "hot_reload", &outManifest->hotReload)) {
    if (outError != nullptr) {
      *outError = "invalid or incomplete manifest";
    }
    return false;
  }

  if (!parseManifestPermissions(manifestJson, &outManifest->permissions,
                                outError)) {
    return false;
  }

  std::string nativeBlock;
  if (!extractJsonBlock(manifestJson, "native", '{', '}', &nativeBlock)) {
    if (outError != nullptr) {
      *outError = "manifest is missing runtime.native block";
    }
    return false;
  }
  if (!extractJsonBool(nativeBlock, "enabled", &outManifest->nativeEnabled)) {
    if (outError != nullptr) {
      *outError = "manifest runtime.native block is missing enabled";
    }
    return false;
  }

  std::string modulesArray;
  if (!extractJsonBlock(nativeBlock, "modules", '[', ']', &modulesArray)) {
    if (outError != nullptr) {
      *outError = "manifest runtime.native block is missing modules";
    }
    return false;
  }

  std::vector<std::string> moduleObjects;
  if (!splitJsonObjectArray(modulesArray, &moduleObjects)) {
    if (outError != nullptr) {
      *outError = "invalid manifest runtime.native.modules array";
    }
    return false;
  }

  for (const auto &moduleObject : moduleObjects) {
    NativeModuleManifestInfo module;
    if (!extractJsonString(moduleObject, "name", &module.name) ||
        !extractJsonString(moduleObject, "abi", &module.abi)) {
      if (outError != nullptr) {
        *outError = "invalid native module entry in manifest";
      }
      return false;
    }

    std::string exportsArray;
    if (!extractJsonBlock(moduleObject, "exports", '[', ']', &exportsArray)) {
      if (outError != nullptr) {
        *outError = "native module '" + module.name +
                    "' is missing exports in manifest";
      }
      return false;
    }
    std::vector<std::string> exportObjects;
    if (!splitJsonObjectArray(exportsArray, &exportObjects)) {
      if (outError != nullptr) {
        *outError = "native module '" + module.name +
                    "' has an invalid exports array";
      }
      return false;
    }
    for (const auto &exportObject : exportObjects) {
      NativeExportInfo exportInfo;
      if (!extractJsonString(exportObject, "name", &exportInfo.name) ||
          !extractJsonString(exportObject, "symbol", &exportInfo.symbol) ||
          !extractJsonString(exportObject, "return", &exportInfo.returnType)) {
        if (outError != nullptr) {
          *outError = "invalid export entry in native module '" + module.name +
                      "'";
        }
        return false;
      }
      std::string paramsArray;
      if (!extractJsonBlock(exportObject, "params", '[', ']', &paramsArray) ||
          !parseJsonStringArrayBody(paramsArray, &exportInfo.parameterTypes)) {
        if (outError != nullptr) {
          *outError = "invalid params entry in native module '" + module.name +
                      "', export '" + exportInfo.name + "'";
        }
        return false;
      }
      module.exports.push_back(std::move(exportInfo));
    }

    std::string artifactsArray;
    if (!extractJsonBlock(moduleObject, "artifacts", '[', ']', &artifactsArray)) {
      if (outError != nullptr) {
        *outError = "native module '" + module.name +
                    "' is missing artifacts in manifest";
      }
      return false;
    }
    std::vector<std::string> artifactObjects;
    if (!splitJsonObjectArray(artifactsArray, &artifactObjects)) {
      if (outError != nullptr) {
        *outError = "native module '" + module.name +
                    "' has an invalid artifacts array";
      }
      return false;
    }
    for (const auto &artifactObject : artifactObjects) {
      NativeArtifactInfo artifact;
      if (!extractJsonString(artifactObject, "platform", &artifact.platform) ||
          !extractJsonString(artifactObject, "resource_id",
                             &artifact.resourceId) ||
          !extractJsonString(artifactObject, "file_name", &artifact.fileName) ||
          !extractJsonString(artifactObject, "sha256", &artifact.sha256) ||
          !extractJsonUInt64(artifactObject, "size", &artifact.size) ||
          !extractJsonUInt32(artifactObject, "crc32", &artifact.crc32)) {
        if (outError != nullptr) {
          *outError = "invalid artifact entry in native module '" + module.name +
                      "'";
        }
        return false;
      }
      module.artifacts.push_back(std::move(artifact));
    }

    outManifest->nativeModules.push_back(std::move(module));
  }

  return true;
}

} // namespace neuron::ncon

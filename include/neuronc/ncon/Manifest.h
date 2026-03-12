#pragma once

#include "neuronc/ncon/Permissions.h"

#include <cstdint>
#include <string>
#include <vector>

namespace neuron::ncon {

struct ResourceInfo {
  std::string id;
  std::string sourcePath;
  uint64_t size = 0;
  uint32_t crc32 = 0;
  uint64_t blobOffset = 0;
};

struct NativeExportInfo {
  std::string name;
  std::string symbol;
  std::vector<std::string> parameterTypes;
  std::string returnType = "void";
};

struct NativeArtifactInfo {
  std::string platform;
  std::string resourceId;
  std::string fileName;
  uint64_t size = 0;
  uint32_t crc32 = 0;
  std::string sha256;
};

struct NativeModuleManifestInfo {
  std::string name;
  std::string abi = "c";
  std::vector<NativeExportInfo> exports;
  std::vector<NativeArtifactInfo> artifacts;
};

struct ManifestData {
  std::string appName;
  std::string appVersion;
  std::string entryModule;
  std::string entryFunction = "Init";
  std::string sourceHash;
  std::string optimize;
  std::string targetCPU;
  std::string tensorProfile;
  bool tensorAutotune = true;
  bool hotReload = false;
  bool nativeEnabled = false;
  neuron::NconPermissionConfig permissions;
  std::vector<ResourceInfo> resources;
  std::vector<NativeModuleManifestInfo> nativeModules;
};

std::string buildManifestJson(const ManifestData &manifest);
bool parseManifestPermissions(const std::string &manifestJson,
                              neuron::NconPermissionConfig *outPermissions,
                              std::string *outError);
bool parseManifestData(const std::string &manifestJson, ManifestData *outManifest,
                       std::string *outError);

} // namespace neuron::ncon

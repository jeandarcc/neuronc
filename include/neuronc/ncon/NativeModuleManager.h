#pragma once

#include "neuronc/ncon/Format.h"
#include "neuronc/ncon/Manifest.h"
#include "neuronc/ncon/Values.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace neuron::ncon {

struct SandboxContext;

class NativeModuleManager {
public:
  NativeModuleManager();
  ~NativeModuleManager();

  bool load(const ContainerData &container, const SandboxContext *sandbox,
            std::string *outError);
  void unload();

  bool hasCallTarget(const std::string &callTarget) const;
  bool invoke(const std::string &callTarget, const std::vector<VMValue> &args,
              VMValue *outValue, std::string *outError) const;

private:
  struct LoadedLibrary;
  struct LoadedExport;

  bool loadModule(const ContainerData &container,
                  const NativeModuleManifestInfo &module,
                  std::string *outError);

  std::vector<LoadedLibrary> m_libraries;
  std::unordered_map<std::string, LoadedExport> m_exports;
  std::filesystem::path m_cacheRoot;
};

} // namespace neuron::ncon

#pragma once

#include "neuronc/ncon/Program.h"
#include "neuronc/ncon/Sandbox.h"
#include "neuronc/ncon/Values.h"

#include <memory>
#include <string>
#include <vector>

namespace neuron::ncon {

class NativeModuleManager;
struct ContainerData;

class RuntimeBridge {
public:
  explicit RuntimeBridge(const SandboxContext *sandbox = nullptr);
  ~RuntimeBridge();

  void bindSandbox(const SandboxContext *sandbox);
  bool isBuiltin(const std::string &functionName) const;

  bool startup(const ContainerData &container, const std::string &moduleName,
               std::string *outError);
  void shutdown();

  bool invokeBuiltin(const Program &program, const std::string &functionName,
                     const std::vector<VMValue> &args, VMValue *outValue,
                     std::string *outError) const;
  bool isNativeCall(const std::string &functionName) const;
  bool invokeNative(const std::string &functionName,
                    const std::vector<VMValue> &args, VMValue *outValue,
                    std::string *outError) const;

private:
  const SandboxContext *m_sandbox = nullptr;
  std::unique_ptr<NativeModuleManager> m_nativeModules;
};

} // namespace neuron::ncon

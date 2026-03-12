#include "neuronc/ncon/NativeModuleManager.h"
#include "neuronc/ncon/RuntimeBridge.h"
#include "neuronc/fusion/FusionBuiltins.h"

#include "neuron_nn.h"
#include "neuron_runtime.h"
#include "neuron_tensor.h"

#include <cmath>
#include <fstream>
#include <unordered_set>

namespace neuron::ncon {

namespace {

const std::unordered_set<std::string> &builtinNames() {
  static const std::unordered_set<std::string> names = {
      "Print",
      "System.Print",
      "IO.WriteLine",
      "IO.ReadInt",
      "Math.Sqrt",
      "Math.Abs",
      "Math.Pow",
      "Time.Now",
      "Random.Int",
      "Random.Float",
      "Logger.Info",
      "Logger.Warning",
      "Logger.Error",
      "__neuron_throw",
      "__neuron_last_exception",
      "__neuron_clear_exception",
      "__neuron_has_exception",
      "Tensor.Random",
      "Tensor.Zeros",
      "Tensor.Ones",
      "Tensor.Identity",
      "create_tensor",
      "NN.SelfTest",
      fusionBuiltinName(FusionBuiltinKind::Conv2DBatchNormRelu),
      "Resource.Exists",
      "Resource.ReadText",
      "Resource.ReadBytes",
  };
  return names;
}

int64_t toInt(const VMValue &value) {
  if (const auto *v = std::get_if<int64_t>(&value.data)) {
    return *v;
  }
  if (const auto *v = std::get_if<double>(&value.data)) {
    return static_cast<int64_t>(*v);
  }
  if (const auto *v = std::get_if<std::string>(&value.data)) {
    return v->empty() ? 0 : 1;
  }
  if (const auto *v = std::get_if<TensorHandle>(&value.data)) {
    return *v == nullptr ? 0 : 1;
  }
  if (const auto *v = std::get_if<PointerHandle>(&value.data)) {
    return *v ? 1 : 0;
  }
  if (const auto *v = std::get_if<ArrayIntHandle>(&value.data)) {
    return (*v && !(*v)->empty()) ? 1 : 0;
  }
  return 0;
}

double toDouble(const VMValue &value) {
  if (const auto *v = std::get_if<double>(&value.data)) {
    return *v;
  }
  if (const auto *v = std::get_if<int64_t>(&value.data)) {
    return static_cast<double>(*v);
  }
  return 0.0;
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
  if (const auto *v = std::get_if<TensorHandle>(&value.data)) {
    return *v == nullptr ? "<null tensor>" : "<tensor>";
  }
  if (const auto *v = std::get_if<PointerHandle>(&value.data)) {
    return *v ? "<ptr>" : "<null>";
  }
  if (const auto *v = std::get_if<ClassObjectHandle>(&value.data)) {
    return *v ? "<object>" : "<null object>";
  }
  if (const auto *v = std::get_if<ArrayIntHandle>(&value.data)) {
    return *v ? ("<array size=" + std::to_string((*v)->size()) + ">")
              : "<null array>";
  }
  return "";
}

NeuronTensor *toTensor(const VMValue &value) {
  if (const auto *v = std::get_if<TensorHandle>(&value.data)) {
    return *v;
  }
  return nullptr;
}

NeuronTensor *createTensor2D(int32_t rows, int32_t cols, float fillValue,
                             bool identity) {
  if (rows <= 0 || cols <= 0) {
    return nullptr;
  }
  int32_t shape[] = {rows, cols};
  NeuronTensor *tensor = neuron_tensor_create(2, shape);
  if (tensor == nullptr) {
    return nullptr;
  }
  for (int32_t i = 0; i < tensor->size; ++i) {
    tensor->data[i] = fillValue;
  }
  if (identity) {
    const int32_t diagonal = rows < cols ? rows : cols;
    for (int32_t i = 0; i < diagonal; ++i) {
      tensor->data[i * cols + i] = 1.0f;
    }
  }
  return tensor;
}

bool readTextFile(const std::filesystem::path &path, std::string *outText,
                  std::string *outError) {
  if (outText == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null text output";
    }
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open resource: " + path.string();
    }
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (!in.good() && !in.eof()) {
    if (outError != nullptr) {
      *outError = "failed to read resource: " + path.string();
    }
    return false;
  }
  *outText = buffer.str();
  return true;
}

bool readBinaryFile(const std::filesystem::path &path,
                    ArrayIntHandle *outBytes,
                    std::string *outError) {
  if (outBytes == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null byte array output";
    }
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open resource: " + path.string();
    }
    return false;
  }
  const auto bytes = std::make_shared<std::vector<int64_t>>();
  char ch = '\0';
  while (in.get(ch)) {
    bytes->push_back(static_cast<unsigned char>(ch));
  }
  if (!in.eof()) {
    if (outError != nullptr) {
      *outError = "failed to read resource: " + path.string();
    }
    return false;
  }
  *outBytes = bytes;
  return true;
}

std::string normalizeResourceId(const std::string &value) {
  if (value.rfind("res:/", 0) == 0) {
    return value.substr(5);
  }
  return value;
}

} // namespace

RuntimeBridge::RuntimeBridge(const SandboxContext *sandbox) : m_sandbox(sandbox) {}

RuntimeBridge::~RuntimeBridge() = default;

void RuntimeBridge::bindSandbox(const SandboxContext *sandbox) {
  m_sandbox = sandbox;
}

bool RuntimeBridge::isBuiltin(const std::string &functionName) const {
  return builtinNames().find(functionName) != builtinNames().end();
}

bool RuntimeBridge::startup(const ContainerData &container,
                            const std::string &moduleName,
                            std::string *outError) {
  if (m_nativeModules == nullptr) {
    m_nativeModules = std::make_unique<NativeModuleManager>();
  }
  if (!m_nativeModules->load(container, m_sandbox, outError)) {
    return false;
  }
  neuron_runtime_startup();
  neuron_module_init(moduleName.c_str());
  return true;
}

void RuntimeBridge::shutdown() {
  neuron_runtime_shutdown();
  if (m_nativeModules != nullptr) {
    m_nativeModules->unload();
  }
}

bool RuntimeBridge::invokeBuiltin(const Program &program,
                                  const std::string &functionName,
                                  const std::vector<VMValue> &args,
                                  VMValue *outValue,
                                  std::string *outError) const {
  if (outValue != nullptr) {
    outValue->data = std::monostate{};
  }

  (void)program;

  if (!isBuiltin(functionName)) {
    return false;
  }

  if ((functionName == "Print" || functionName == "System.Print") &&
      args.size() >= 1) {
    if (std::holds_alternative<int64_t>(args[0].data)) {
      neuron_system_print_int(toInt(args[0]));
    } else {
      const std::string text = toString(args[0]);
      neuron_system_print_str(text.c_str());
    }
    return true;
  }

  if (functionName == "IO.WriteLine" && args.size() >= 1) {
    const std::string text = toString(args[0]);
    neuron_io_write_line(text.c_str());
    return true;
  }
  if (functionName == "IO.ReadInt") {
    if (outValue != nullptr) {
      outValue->data = neuron_io_read_int();
    }
    return true;
  }
  if (functionName == "Math.Sqrt" && args.size() >= 1) {
    if (outValue != nullptr) {
      outValue->data = neuron_math_sqrt(toDouble(args[0]));
    }
    return true;
  }
  if (functionName == "Math.Abs" && args.size() >= 1) {
    if (outValue != nullptr) {
      outValue->data = neuron_math_abs(toDouble(args[0]));
    }
    return true;
  }
  if (functionName == "Math.Pow" && args.size() >= 2) {
    if (outValue != nullptr) {
      outValue->data = neuron_math_pow(toDouble(args[0]), toDouble(args[1]));
    }
    return true;
  }
  if (functionName == "Time.Now") {
    if (outValue != nullptr) {
      outValue->data = neuron_time_now_ms();
    }
    return true;
  }
  if (functionName == "Random.Int" && args.size() >= 2) {
    if (outValue != nullptr) {
      outValue->data = neuron_random_int(toInt(args[0]), toInt(args[1]));
    }
    return true;
  }
  if (functionName == "Random.Float") {
    if (outValue != nullptr) {
      outValue->data = neuron_random_float();
    }
    return true;
  }
  if (functionName == "Logger.Info" && args.size() >= 1) {
    const std::string text = toString(args[0]);
    neuron_log_info(text.c_str());
    return true;
  }
  if (functionName == "Logger.Warning" && args.size() >= 1) {
    const std::string text = toString(args[0]);
    neuron_log_warning(text.c_str());
    return true;
  }
  if (functionName == "Logger.Error" && args.size() >= 1) {
    const std::string text = toString(args[0]);
    neuron_log_error(text.c_str());
    return true;
  }
  if (functionName == "__neuron_throw" && args.size() >= 1) {
    const std::string text = toString(args[0]);
    neuron_throw(text.c_str());
    return true;
  }
  if (functionName == "__neuron_last_exception") {
    const char *text = neuron_last_exception();
    if (outValue != nullptr) {
      outValue->data = text == nullptr ? std::string() : std::string(text);
    }
    return true;
  }
  if (functionName == "__neuron_clear_exception") {
    neuron_clear_exception();
    return true;
  }
  if (functionName == "__neuron_has_exception") {
    if (outValue != nullptr) {
      outValue->data = neuron_has_exception();
    }
    return true;
  }
  if (functionName == "Tensor.Random") {
    NeuronTensor *tensor = nullptr;
    if (args.size() >= 2) {
      tensor = neuron_tensor_random_2d(static_cast<int32_t>(toInt(args[0])),
                                       static_cast<int32_t>(toInt(args[1])));
    } else if (args.size() == 1) {
      const int32_t n = static_cast<int32_t>(toInt(args[0]));
      tensor = neuron_tensor_random_2d(n, n);
    } else {
      tensor = neuron_tensor_create_default();
    }
    if (outValue != nullptr) {
      outValue->data = tensor;
    }
    return true;
  }
  if (functionName == "Tensor.Zeros" || functionName == "Tensor.Ones" ||
      functionName == "Tensor.Identity") {
    const int32_t rows =
        args.size() >= 1 ? static_cast<int32_t>(toInt(args[0])) : 3;
    const int32_t cols =
        args.size() >= 2 ? static_cast<int32_t>(toInt(args[1])) : rows;
    const float fillValue = functionName == "Tensor.Ones" ? 1.0f : 0.0f;
    if (outValue != nullptr) {
      outValue->data = createTensor2D(
          rows, cols, fillValue, functionName == "Tensor.Identity");
    }
    return true;
  }
  if (functionName == "create_tensor") {
    if (outValue != nullptr) {
      outValue->data = neuron_tensor_create_default();
    }
    return true;
  }
  if (functionName == "NN.SelfTest") {
    if (outValue != nullptr) {
      outValue->data = neuron_nn_self_test();
    }
    return true;
  }
  if (functionName ==
      fusionBuiltinName(FusionBuiltinKind::Conv2DBatchNormRelu)) {
    if (args.size() < 13) {
      if (outError != nullptr) {
        *outError = "fused Conv2D-BatchNorm-ReLU requires 13 arguments";
      }
      return false;
    }
    NeuronTensor *tensor = neuron_tensor_conv2d_batchnorm_relu_ex_hint(
        toTensor(args[0]), toTensor(args[1]), toTensor(args[2]),
        toTensor(args[3]), toTensor(args[4]), toTensor(args[5]),
        toTensor(args[6]), static_cast<float>(toDouble(args[7])),
        static_cast<int32_t>(toInt(args[8])),
        static_cast<int32_t>(toInt(args[9])),
        static_cast<int32_t>(toInt(args[10])),
        static_cast<int32_t>(toInt(args[11])),
        static_cast<NeuronTensorExecHint>(toInt(args[12])));
    if (outValue != nullptr) {
      outValue->data = tensor;
    }
    if (tensor == nullptr && outError != nullptr) {
      *outError = "fused Conv2D-BatchNorm-ReLU execution failed";
    }
    return tensor != nullptr;
  }
  if (functionName == "Resource.Exists") {
    if (m_sandbox == nullptr) {
      if (outError != nullptr) {
        *outError = "resource builtin requires sandbox context";
      }
      return false;
    }
    const std::string resourceId = args.empty() ? std::string() : normalizeResourceId(toString(args[0]));
    const std::string logicalPath = "res:/" + resourceId;
    if (!isLogicalPathAllowed(*m_sandbox, logicalPath, SandboxAccessMode::Read,
                              outError)) {
      return false;
    }
    const auto mounted = m_sandbox->mountedResources.find(resourceId);
    if (outValue != nullptr) {
      outValue->data = int64_t{mounted != m_sandbox->mountedResources.end() ? 1 : 0};
    }
    return true;
  }
  if (functionName == "Resource.ReadText") {
    if (m_sandbox == nullptr) {
      if (outError != nullptr) {
        *outError = "resource builtin requires sandbox context";
      }
      return false;
    }
    const std::string resourceId = args.empty() ? std::string() : normalizeResourceId(toString(args[0]));
    std::filesystem::path path;
    if (!resolveResourcePath(*m_sandbox, resourceId, &path, outError)) {
      return false;
    }
    std::string text;
    if (!readTextFile(path, &text, outError)) {
      return false;
    }
    if (outValue != nullptr) {
      outValue->data = text;
    }
    return true;
  }
  if (functionName == "Resource.ReadBytes") {
    if (m_sandbox == nullptr) {
      if (outError != nullptr) {
        *outError = "resource builtin requires sandbox context";
      }
      return false;
    }
    const std::string resourceId = args.empty() ? std::string() : normalizeResourceId(toString(args[0]));
    std::filesystem::path path;
    if (!resolveResourcePath(*m_sandbox, resourceId, &path, outError)) {
      return false;
    }
    ArrayIntHandle bytes;
    if (!readBinaryFile(path, &bytes, outError)) {
      return false;
    }
    if (outValue != nullptr) {
      outValue->data = bytes;
    }
    return true;
  }

  return false;
}

bool RuntimeBridge::isNativeCall(const std::string &functionName) const {
  return m_nativeModules != nullptr && m_nativeModules->hasCallTarget(functionName);
}

bool RuntimeBridge::invokeNative(const std::string &functionName,
                                 const std::vector<VMValue> &args,
                                 VMValue *outValue,
                                 std::string *outError) const {
  if (m_nativeModules == nullptr) {
    if (outError != nullptr) {
      *outError = "native module manager is not initialized";
    }
    return false;
  }
  return m_nativeModules->invoke(functionName, args, outValue, outError);
}

} // namespace neuron::ncon

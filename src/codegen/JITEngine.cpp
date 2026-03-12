#include "neuronc/codegen/JITEngine.h"

#include <llvm/ExecutionEngine/Orc/AbsoluteSymbols.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>

#include <sstream>
#include <string_view>
#include <utility>

namespace neuron {

namespace {

constexpr const char *kRuntimeSymbols[] = {
    "neuron_runtime_startup",
    "neuron_runtime_shutdown",
    "neuron_module_init",
    "neuron_thread_submit",
    "neuron_gpu_scope_begin",
    "neuron_gpu_scope_begin_ex",
    "neuron_gpu_scope_end",
    "neuron_print_int",
    "neuron_print_str",
    "neuron_repl_echo_string",
    "neuron_math_sqrt",
    "neuron_math_pow",
    "neuron_math_abs",
    "neuron_io_write_line",
    "neuron_io_read_int",
    "neuron_io_input_int",
    "neuron_io_input_float",
    "neuron_io_input_double",
    "neuron_io_input_bool",
    "neuron_io_input_string",
    "neuron_io_input_enum",
    "neuron_time_now_ms",
    "neuron_random_int",
    "neuron_random_float",
    "neuron_log_info",
    "neuron_log_warning",
    "neuron_log_error",
    "neuron_throw",
    "neuron_last_exception",
    "neuron_clear_exception",
    "neuron_has_exception",
    "neuron_tensor_add_ex",
    "neuron_tensor_sub_ex",
    "neuron_tensor_mul_ex",
    "neuron_tensor_div_ex",
    "neuron_tensor_fma_ex",
    "neuron_tensor_matmul_ex_hint",
    "neuron_tensor_matmul_add_ex_hint",
    "neuron_tensor_linear_fused_ex_hint",
    "neuron_tensor_conv2d_batchnorm_relu_ex_hint",
    "neuron_tensor_create_default",
    "neuron_tensor_random_2d",
    "neuron_graphics_create_window",
    "neuron_graphics_create_canvas",
    "neuron_graphics_canvas_free",
    "neuron_graphics_canvas_pump",
    "neuron_graphics_canvas_should_close",
    "neuron_graphics_canvas_take_resize",
    "neuron_graphics_canvas_begin_frame",
    "neuron_graphics_canvas_end_frame",
    "neuron_graphics_texture_load",
    "neuron_graphics_mesh_load",
    "neuron_graphics_draw",
    "neuron_graphics_clear",
    "neuron_graphics_present",
    "neuron_graphics_window_get_width",
    "neuron_graphics_window_get_height",
    "neuron_graphics_last_error",
    "neuron_nn_self_test",
    "neuron_modulecpp_register",
    "neuron_modulecpp_startup",
    "neuron_modulecpp_shutdown",
    "neuron_modulecpp_invoke",
};

void jitNoOpMainStub() {}

std::string formatLlvmError(llvm::Error error) {
  std::string message;
  llvm::raw_string_ostream stream(message);
  llvm::logAllUnhandledErrors(std::move(error), stream, "");
  return stream.str();
}

const char *compatibleRuntimeAlias(const char *symbolName) {
  if (symbolName == nullptr) {
    return nullptr;
  }
  if (std::string_view(symbolName) == "neuron_print_int") {
    return "neuron_system_print_int";
  }
  if (std::string_view(symbolName) == "neuron_print_str") {
    return "neuron_system_print_str";
  }
  return nullptr;
}

bool loadRuntimeSymbolsFromLibrary(llvm::sys::DynamicLibrary library,
                                   std::unordered_map<std::string, void *> *outSymbols,
                                   std::string *outError) {
  if (outSymbols == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null JIT symbol output";
    }
    return false;
  }

  outSymbols->clear();
  for (const char *symbolName : kRuntimeSymbols) {
    void *address = library.getAddressOfSymbol(symbolName);
    if (address == nullptr) {
      if (const char *alias = compatibleRuntimeAlias(symbolName);
          alias != nullptr) {
        address = library.getAddressOfSymbol(alias);
      }
    }
    if (address == nullptr) {
      if (outError != nullptr) {
        *outError = "missing runtime symbol for JIT: " + std::string(symbolName);
      }
      outSymbols->clear();
      return false;
    }
    (*outSymbols)[symbolName] = address;
  }
  return true;
}

} // namespace

bool CurrentProcessJitSymbolProvider::loadSymbols(
    std::unordered_map<std::string, void *> *outSymbols, std::string *outError) {
  std::string loadError;
  llvm::sys::DynamicLibrary library =
      llvm::sys::DynamicLibrary::getPermanentLibrary(nullptr, &loadError);
  if (!library.isValid()) {
    if (outError != nullptr) {
      *outError = loadError.empty() ? "failed to open current process symbol table"
                                    : loadError;
    }
    return false;
  }
  return loadRuntimeSymbolsFromLibrary(std::move(library), outSymbols, outError);
}

SharedLibraryJitSymbolProvider::SharedLibraryJitSymbolProvider(
    std::filesystem::path libraryPath)
    : m_libraryPath(std::move(libraryPath)) {}

bool SharedLibraryJitSymbolProvider::loadSymbols(
    std::unordered_map<std::string, void *> *outSymbols, std::string *outError) {
  std::string loadError;
  const std::string libraryPathString = m_libraryPath.string();
  llvm::sys::DynamicLibrary library = llvm::sys::DynamicLibrary::getPermanentLibrary(
      libraryPathString.c_str(), &loadError);
  if (!library.isValid()) {
    if (outError != nullptr) {
      *outError = loadError.empty() ? "failed to load runtime library '" +
                                          libraryPathString + "'"
                                    : loadError;
    }
    return false;
  }
  return loadRuntimeSymbolsFromLibrary(std::move(library), outSymbols, outError);
}

JITEngine::JITEngine() = default;
JITEngine::~JITEngine() = default;

bool JITEngine::initialize(std::unique_ptr<JitSymbolProvider> symbolProvider,
                           std::string *outError) {
  if (symbolProvider == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null JIT symbol provider";
    }
    return false;
  }

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto expectedJit =
      llvm::orc::LLJITBuilder()
          .setLinkProcessSymbolsByDefault(true)
          .setPlatformSetUp(llvm::orc::setUpInactivePlatform)
          .create();
  if (!expectedJit) {
    if (outError != nullptr) {
      *outError = formatLlvmError(expectedJit.takeError());
    }
    return false;
  }

  m_symbolProvider = std::move(symbolProvider);
  m_jit = std::move(*expectedJit);

  std::unordered_map<std::string, void *> symbols;
  if (!m_symbolProvider->loadSymbols(&symbols, outError)) {
    m_jit.reset();
    m_symbolProvider.reset();
    return false;
  }
  symbols["__main"] = reinterpret_cast<void *>(&jitNoOpMainStub);

  llvm::orc::SymbolMap symbolMap;
  for (const auto &entry : symbols) {
    symbolMap[m_jit->mangleAndIntern(entry.first)] = llvm::orc::ExecutorSymbolDef(
        llvm::orc::ExecutorAddr::fromPtr(entry.second),
        llvm::JITSymbolFlags::Exported);
  }

  if (llvm::Error err =
          m_jit->getMainJITDylib().define(llvm::orc::absoluteSymbols(std::move(symbolMap)))) {
    if (outError != nullptr) {
      *outError = formatLlvmError(std::move(err));
    }
    m_jit.reset();
    m_symbolProvider.reset();
    return false;
  }

  return true;
}

bool JITEngine::addModule(OwnedLLVMModule module, std::string *outError) {
  if (m_jit == nullptr) {
    if (outError != nullptr) {
      *outError = "JIT engine is not initialized";
    }
    return false;
  }
  if (module.context == nullptr || module.module == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: invalid LLVM module for JIT";
    }
    return false;
  }

  llvm::orc::ThreadSafeModule threadSafeModule(std::move(module.module),
                                               std::move(module.context));
  if (llvm::Error err = m_jit->addIRModule(std::move(threadSafeModule))) {
    if (outError != nullptr) {
      *outError = formatLlvmError(std::move(err));
    }
    return false;
  }
  return true;
}

bool JITEngine::executeMain(int *outExitCode, std::string *outError) {
  if (m_jit == nullptr) {
    if (outError != nullptr) {
      *outError = "JIT engine is not initialized";
    }
    return false;
  }

  auto expectedMain = m_jit->lookup("main");
  if (!expectedMain) {
    if (outError != nullptr) {
      *outError = formatLlvmError(expectedMain.takeError());
    }
    return false;
  }

  auto *mainFn = expectedMain->toPtr<int (*)()>();
  if (mainFn == nullptr) {
    if (outError != nullptr) {
      *outError = "JIT entry point 'main' resolved to null";
    }
    return false;
  }

  const int exitCode = mainFn();
  if (outExitCode != nullptr) {
    *outExitCode = exitCode;
  }
  return true;
}

} // namespace neuron

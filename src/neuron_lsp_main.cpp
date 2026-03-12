#include "lsp/NeuronLspServer.h"

#include <filesystem>
#include <optional>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

bool hasToolRootMarkers(const fs::path &root) {
  std::error_code ec;
  return fs::exists(root / "runtime/src/runtime.c", ec) &&
         fs::exists(root / "runtime/include/neuron_runtime.h", ec);
}

std::optional<fs::path> currentExecutablePath() {
#ifdef _WIN32
  std::vector<wchar_t> buffer(MAX_PATH, L'\0');
  for (;;) {
    const DWORD written = GetModuleFileNameW(nullptr, buffer.data(),
                                             static_cast<DWORD>(buffer.size()));
    if (written == 0) {
      return std::nullopt;
    }
    if (written < buffer.size() - 1) {
      return fs::path(std::wstring(buffer.data(), written));
    }
    buffer.resize(buffer.size() * 2, L'\0');
  }
#elif defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  if (size == 0) {
    return std::nullopt;
  }
  std::vector<char> buffer(size + 1, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
    return std::nullopt;
  }
  return fs::path(buffer.data());
#else
  std::vector<char> buffer(4096, '\0');
  for (;;) {
    const ssize_t written =
        readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (written < 0) {
      return std::nullopt;
    }
    if (static_cast<std::size_t>(written) < buffer.size() - 1) {
      buffer[static_cast<std::size_t>(written)] = '\0';
      return fs::path(buffer.data());
    }
    buffer.resize(buffer.size() * 2, '\0');
  }
#endif
}

fs::path detectToolRoot() {
  const std::optional<fs::path> exePath = currentExecutablePath();
  if (exePath.has_value()) {
    fs::path probe = exePath->parent_path();
    for (int depth = 0; depth < 6; ++depth) {
      if (hasToolRootMarkers(probe)) {
        return probe;
      }
      const fs::path parent = probe.parent_path();
      if (parent == probe) {
        break;
      }
      probe = parent;
    }
  }
  return fs::current_path();
}

} // namespace

int main() {
  neuron::lsp::NeuronLspServer server(detectToolRoot());
  return server.run();
}

// Cmd_Surgeon.cpp — Toolchain health report and installation guide command.
//
// This file contains:
//   runSurgeonReport   -> checks toolchain components, prints [OK]/[WARN]/[FAIL]
//   runSurgeonInstall  -> prints installation instructions for a missing component
//   cmdSurgeon         -> top-level command router
//
// To add a new component check:
//   1. Add a new check block inside runSurgeonReport().
//   2. Add a matching entry inside runSurgeonInstall().
#include "CommandHandlers.h"
#include "AppGlobals.h"
#include "ToolchainUtils.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace {

bool findExecutable(const std::string &name) {
  if (!g_toolchainBinDir.empty()) {
    if (hasToolExecutableAt(fs::path(g_toolchainBinDir), name)) {
      return true;
    }
  }
  if (!g_toolRoot.empty()) {
    if (hasToolExecutableAt(g_toolRoot / "bin", name)) {
      return true;
    }
  }
#ifdef _WIN32
  std::string cmd = "where " + name + " >NUL 2>&1";
#else
  std::string cmd = "which " + name + " >/dev/null 2>&1";
#endif
  return system(cmd.c_str()) == 0;
}

const char *vulkanSdkEnv() {
  return std::getenv("VULKAN_SDK");
}

} // namespace

// ── Health report ─────────────────────────────────────────────────────────────

static int runSurgeonReport() {
  bool anyFail = false;
  bool anyWarn = false;

  std::cout << "\nNeuron++ Surgical Report\n";
  std::cout << "------------------------------------\n";

  // Compiler (neuron binary)
  {
    bool found = findExecutable("neuron");
    if (found) {
      std::cout << "[OK]   Compiler            neuron binary found\n";
    } else {
      std::cout << "[FAIL] Compiler            neuron binary not found\n";
      anyFail = true;
    }
  }

  // LLVM (llc)
  {
    bool found = findExecutable("llc");
    if (found) {
      std::cout << "[OK]   LLVM backend        llc found\n";
    } else {
      std::cout << "[FAIL] LLVM backend        llc not found -- LLVM is required for codegen\n";
      anyFail = true;
    }
  }

  // Toolchain (gcc)
  {
    bool configured = !g_toolchainBinDir.empty();
    bool gccFound   = findExecutable("gcc");
    if (configured && gccFound) {
      std::cout << "[OK]   Toolchain           gcc found in " << g_toolchainBinDir << "\n";
    } else if (gccFound) {
      std::cout << "[OK]   Toolchain           gcc found on PATH\n";
    } else {
      std::cout << "[WARN] Toolchain           gcc not found\n";
      std::cout << "                           run: neuron surgeon install toolchain\n";
      anyWarn = true;
    }
  }

  // LSP (neuron-lsp)
  {
    bool found = findExecutable("neuron-lsp");
    if (found) {
      std::cout << "[OK]   neuron-lsp          binary found\n";
    } else {
      std::cout << "[WARN] neuron-lsp          not found -- editor integration unavailable\n";
      std::cout << "                           run: neuron surgeon install lsp\n";
      anyWarn = true;
    }
  }

  // Vulkan SDK
  {
    const char *sdkEnv   = vulkanSdkEnv();
    bool        envSet   = sdkEnv != nullptr && sdkEnv[0] != '\0';
    bool        infoTool = findExecutable("vulkaninfo");

    if (envSet && infoTool) {
      std::cout << "[OK]   Vulkan SDK          VULKAN_SDK set, vulkaninfo found\n";
    } else if (envSet && !infoTool) {
      std::cout << "[WARN] Vulkan SDK          VULKAN_SDK set but vulkaninfo not on PATH\n";
      anyWarn = true;
    } else {
      std::cout << "[FAIL] Vulkan SDK          not found -- graphics unavailable\n";
      std::cout << "                           run: neuron surgeon install vulkan\n";
      anyFail = true;
    }
  }

  std::cout << "------------------------------------\n";

  if (anyFail) {
    std::cout << "Status: some components are missing. See [FAIL] items above.\n\n";
    return 1;
  }
  if (anyWarn) {
    std::cout << "Status: healthy with warnings. See [WARN] items above.\n\n";
    return 0;
  }
  std::cout << "Status: all components healthy.\n\n";
  return 0;
}

// ── Installation instructions ─────────────────────────────────────────────────

static int runSurgeonInstall(const std::string &component) {
  if (component == "vulkan" || component == "vk") {
    std::cout << "\n[vulkan] Vulkan SDK Installation\n";
    std::cout << "------------------------------------\n";
    std::cout << "1. Visit the LunarG Vulkan SDK page:\n";
    std::cout << "     https://vulkan.lunarg.com/sdk/home\n";
    std::cout << "2. Download and run the installer for your OS.\n";
    std::cout << "3. Verify VULKAN_SDK environment variable is set:\n";
#ifdef _WIN32
    std::cout << "     echo %VULKAN_SDK%\n";
#else
    std::cout << "     echo $VULKAN_SDK\n";
#endif
    std::cout << "4. Test the installation:\n";
    std::cout << "     vulkaninfo\n\n";
    return 0;
  }

  if (component == "lsp") {
    std::cout << "\n[lsp] neuron-lsp Installation\n";
    std::cout << "------------------------------------\n";
    std::cout << "1. Download neuron-lsp from the Neuron++ releases page:\n";
    std::cout << "     https://github.com/neuronpp/neuron/releases\n";
    std::cout << "2. Copy the binary to a directory on your PATH.\n";
#ifdef _WIN32
    std::cout << "   Example:\n";
    std::cout << "     copy neuron-lsp.exe C:\\neuron\\bin\\\n";
#else
    std::cout << "   Example:\n";
    std::cout << "     cp neuron-lsp /usr/local/bin/\n";
#endif
    std::cout << "3. Verify the installation:\n";
    std::cout << "     neuron-lsp --version\n";
    std::cout << "4. For IDE integration see: extensions/vscode-npp\n\n";
    return 0;
  }

  if (component == "toolchain") {
    std::cout << "\n[toolchain] GCC Toolchain Installation\n";
    std::cout << "------------------------------------\n";
    std::cout << "Neuron++ requires GCC 12+ and binutils.\n\n";
#ifdef _WIN32
    std::cout << "Windows:\n";
    std::cout << "  1. Install MSYS2: https://www.msys2.org\n";
    std::cout << "  2. In the MSYS2 terminal:\n";
    std::cout << "       pacman -S mingw-w64-x86_64-gcc\n";
    std::cout << "  3. Point Neuron++ to the toolchain:\n";
    std::cout << "       set NEURON_TOOLCHAIN_BIN=C:\\msys64\\mingw64\\bin\n\n";
#elif defined(__APPLE__)
    std::cout << "macOS:\n";
    std::cout << "  1. Install Xcode Command Line Tools:\n";
    std::cout << "       xcode-select --install\n";
    std::cout << "  2. Or via Homebrew:\n";
    std::cout << "       brew install gcc\n\n";
#else
    std::cout << "Linux:\n";
    std::cout << "  Ubuntu/Debian:  sudo apt install gcc g++ binutils\n";
    std::cout << "  Fedora/RHEL:    sudo dnf install gcc gcc-c++ binutils\n";
    std::cout << "  Arch:           sudo pacman -S gcc binutils\n\n";
#endif
    return 0;
  }

  if (component == "llvm") {
    std::cout << "\n[llvm] LLVM Installation\n";
    std::cout << "------------------------------------\n";
    std::cout << "Neuron++ requires LLVM 14+.\n\n";
#ifdef _WIN32
    std::cout << "Windows:\n";
    std::cout << "  1. Download the official LLVM installer:\n";
    std::cout << "       https://github.com/llvm/llvm-project/releases\n";
    std::cout << "  2. During setup, check 'Add LLVM to the system PATH'.\n\n";
#elif defined(__APPLE__)
    std::cout << "macOS:\n";
    std::cout << "       brew install llvm\n";
    std::cout << "       echo 'export PATH=\"/opt/homebrew/opt/llvm/bin:$PATH\"' >> ~/.zshrc\n\n";
#else
    std::cout << "Linux:\n";
    std::cout << "  Ubuntu/Debian:  sudo apt install llvm\n";
    std::cout << "  Specific ver:   sudo apt install llvm-14\n\n";
#endif
    return 0;
  }

  // Unknown component
  std::cerr << "Unknown component: " << component << "\n\n";
  std::cerr << "Supported components:\n";
  std::cerr << "  vulkan    -- Vulkan SDK (graphics)\n";
  std::cerr << "  lsp       -- neuron-lsp language server\n";
  std::cerr << "  toolchain -- GCC/binutils toolchain\n";
  std::cerr << "  llvm      -- LLVM backend (llc)\n\n";
  std::cerr << "Usage: neuron surgeon install <component>\n";
  return 1;
}

// ── Top-level command router ──────────────────────────────────────────────────

int cmdSurgeon(int argc, char *argv[]) {
  // neuron surgeon                     -> health report
  // neuron surgeon install             -> usage error
  // neuron surgeon install <component> -> installation instructions

  if (argc < 3) {
    return runSurgeonReport();
  }

  const std::string sub = argv[2];

  if (sub == "install") {
    if (argc < 4) {
      std::cerr << "Usage: neuron surgeon install <component>\n";
      std::cerr << "Supported: vulkan, lsp, toolchain, llvm\n";
      return 1;
    }
    return runSurgeonInstall(argv[3]);
  }

  // Unknown subcommand
  std::cerr << "Unknown surgeon subcommand: " << sub << "\n\n";
  std::cerr << "Usage:\n";
  std::cerr << "  neuron surgeon                     -- print health report\n";
  std::cerr << "  neuron surgeon install <component> -- show install instructions\n";
  return 1;
}


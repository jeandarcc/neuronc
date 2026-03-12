// AppGlobals.cpp — Global değişkenlerin tek tanım noktası (ODR).
// Herhangi bir modülden erişmek için AppGlobals.h'ı include et.
#include "AppGlobals.h"

#include <filesystem>
#include <string>

// g_toolchainBinDir: CMake tarafından derleme zamanında NPP_TOOLCHAIN_BIN_DIR
// olarak sağlanan derleyici bin dizini ile başlatılır.
// initializeToolchainBinDir() çalışma zamanında bunu doğrular veya günceller.
#ifndef NPP_TOOLCHAIN_BIN_DIR
#define NPP_TOOLCHAIN_BIN_DIR ""
#endif

std::string g_toolchainBinDir  = NPP_TOOLCHAIN_BIN_DIR;
fs::path    g_toolRoot         = fs::current_path();
fs::path    g_runtimeObjectDir;
bool        g_traceErrors      = false;
bool        g_colorDiagnostics = false;
bool        g_bypassRules      = false;


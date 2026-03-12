// RuntimePaths.h — Runtime nesne dizinleri ve platform yardımcıları.
//
// Bu modül şu işlemleri sağlar:
//   - Runtime obje önbellek dizinini belirleme/yazılabilirlik kontrolü
//   - Mevcut ana makine platformunu tespit etme
//   - Native artifact (DLL/SO/dylib) adaylarını listeleme ve bulma
//   - ModuleCpp kaynaklarından native kütüphane derleme
//
// Yeni bir platform veya artifact tipi eklemek istersen bu dosyayı genişlet.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// İleri bildirim — modulecpp tipi için tam header gerekmez burada.
namespace neuron {
struct LoadedModuleCppModule;
} // namespace neuron

// ── Directory helpers ───────────────────────────────────────────────────────

/// NEURON_RUNTIME_CACHE_DIR veya platform önbelleğini döner.
fs::path defaultRuntimeObjectCacheDir();

/// Bir dizine yazılabilirlik testi yapar.
bool canWriteToDirectory(const fs::path &dir);

/// Kullanılacak runtime obje dizinini döner; ilk kullanımda hesaplayıp
/// g_runtimeObjectDir'e kaydeder.
fs::path runtimeObjectDirectory();

// ── Platform detection ──────────────────────────────────────────────────────

/// "windows_x64", "linux_x64", "macos_arm64" veya "unsupported" döner.
std::string currentHostPlatform();

// ── Artifact discovery ──────────────────────────────────────────────────────

/// Platform'a göre aday kütüphane adlarını üretir (örn. "foo.dll" /
/// "libfoo.so").
std::vector<fs::path> candidateLibraryNames(const std::string &targetName);

/// buildDir altında rekürsif tarama yaparak targetName'e ait native
/// artifact'ı bulur.
bool findBuiltNativeArtifact(const fs::path &buildDir,
                             const std::string &targetName,
                             fs::path *outArtifact,
                             std::string *outError = nullptr);

// ── ModuleCpp compilation ───────────────────────────────────────────────────

/// Bir modulecpp modülünü CMake ile derler ve artifact yolunu döner.
bool buildModuleCppFromSource(const fs::path &projectRoot,
                              const neuron::LoadedModuleCppModule &module,
                              const std::string &hostPlatform,
                              fs::path *outArtifact,
                              std::string *outError = nullptr);

// SettingsLoader.h — .neuronsettings dosyasını yükleme ve doğrulama.
//
// Bu modül şu işlemleri sağlar:
//   - Env değişkeninden / argümandan trace/bypass flag'lerini işleme
//   - .neuronsettings dosyasını bulma ve ayrıştırma
//   - Kural politikalarını (script docs, root scripts) doğrulama
//   - Bypass-rules ile tüm kural limitlerini sıfırlama
//
// Yeni bir ayar anahtarı eklemek istersen loadNeuronSettings() içindeki
// ayrıştırma bloğunu genişlet ve NeuronSettings struct'ına alan ekle.
#pragma once

#include "neuronc/cli/DebugSupport.h"

#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

using neuron::cli::NeuronSettings;

// ── Flag parsing ────────────────────────────────────────────────────────────

/// "1/true/yes/on" veya "0/false/no/off" değerlerini bool'a çevirir.
bool parseTraceFlagValue(const std::string &value, bool *outEnabled);

/// "--bypass-rules" veya "-bypass-rules" kontrolü.
bool isBypassRulesFlag(const std::string &value);

/// settings nesnesindeki tüm kural limitlerini sıfırlar (bypass modu).
void applyBypassRulesToSettings(NeuronSettings *settings);

/// NEURON_TRACE_ERRORS env değişkeninden g_traceErrors'ı başlatır.
void initializeTraceErrorsFromEnv();

/// NEURON_COLOR / NO_COLOR env'den g_colorDiagnostics'i başlatır.
/// Windows'ta virtual terminal processing'i etkinleştirir.
void initializeDiagnosticColorFromEnv();

// ── Argument parsing ────────────────────────────────────────────────────────

/// argv[startIndex..]'dan dosya argümanını ve trace/bypass flag'lerini
/// ayrıştırır. Hata durumunda nullopt döner.
std::optional<std::string>
parseFileArgWithTraceFlags(int argc, char *argv[], int startIndex,
                           const std::string &usageLine);

// ── Find / load settings file ──────────────────────────────────────────────

/// startDir'den yukarıya çıkarak .neuronsettings dosyasını arar.
std::optional<fs::path> findNearestSettingsFile(const fs::path &startDir);

/// pathHint'e en yakın .neuronsettings'i yükler; bulamazsa varsayılanları
/// döner.
NeuronSettings loadNeuronSettings(const fs::path &pathHint);

// ── Policy verification ──────────────────────────────────────────────────────

/// Kaynak dosya için script politikasını (forbid_root, require_docs) kontrol
/// eder.
bool validateScriptPolicy(const fs::path &sourcePath,
                          const NeuronSettings &settings);

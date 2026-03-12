// ProjectHelpers.h — Proje yapılandırma ve kaynak dosya yardımcıları.
//
// Bu modül şu işlemleri sağlar:
//   - Kaynak dosya okuma (max-lines kontrolü ile)
//   - neuron.toml'dan ProjectConfig yükleme
//   - Kullanılabilir modülleri toplama
//   - SemanticAnalyzer'ı proje ayarlarına göre yapılandırma
//   - Import edilen modulecpp modüllerini AST'den toplama
//
// Yeni bir semantic kural ayarı eklemek istersen
// configureSemanticAnalyzerModules fonksiyonunu güncelle.
#pragma once

#include "SettingsLoader.h"

#include "neuronc/cli/ProjectConfig.h"
#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/parser/Parser.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using neuron::cli::NeuronSettings;

// ── Source file reading ─────────────────────────────────────────────────────

/// Dosyayı okur; max_lines_per_file ve script policy kontrolü yapar.
/// Hata durumunda boş string döner.
std::string readFile(const std::string &path, const NeuronSettings &settings);

// ── Project configuration ───────────────────────────────────────────────────

/// CWD'deki neuron.toml'u yükler. Hata varsa outErrors'a ekler.
bool loadProjectConfigFromCwd(neuron::ProjectConfig *outConfig,
                              std::vector<std::string> *outErrors);

/// CWD'deki neuron.toml'u yükler; hata durumunda nullopt döner.
std::optional<neuron::ProjectConfig> tryLoadProjectConfigFromCwd();

// ── Module collection ───────────────────────────────────────────────────────

/// Bir AST'deki import edilen modulecpp modül adlarını toplar.
std::unordered_set<std::string>
collectImportedModuleCppModules(const neuron::ProgramNode *program);

/// Proje dizinlerinden ve config'den kullanılabilir modül adlarını toplar.
std::unordered_set<std::string>
collectAvailableModules(const fs::path &sourceFile,
                        const std::optional<neuron::ProjectConfig> &config);

// ── SemanticAnalyzer configuration ──────────────────────────────────────────

/// SemanticAnalyzer'ı proje ayarları + modulecpp bilgisiyle yapılandırır.
void configureSemanticAnalyzerModules(
    neuron::SemanticAnalyzer *sema, const fs::path &sourceFile,
    const std::optional<neuron::ProjectConfig> &config,
    const NeuronSettings &settings, const std::string &sourceText,
    std::vector<std::string> *outConfigErrors = nullptr);

// ── NIR helpers ─────────────────────────────────────────────────────────────

/// Bir NIR modülündeki toplam komut sayısını döner.
std::size_t countNIRInstructions(const neuron::nir::Module *module);

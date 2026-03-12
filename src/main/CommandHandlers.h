// CommandHandlers.h — CLI komut işleyicilerinin ortak arayüzü.
//
// Implementasyonlar 3 dosyaya bölünmüştür:
//   Cmd_Package.cpp  → paket yönetimi + proje oluşturma
//   Cmd_Debug.cpp    → lex/parse/nir/compile + usage metni
//   Cmd_Build.cpp    → build-nucleus / build-product / build / run / release
//
// Yardım metni (usage) → UsageText.h — SADECE orayı düzenle, buraya dokunma.
//
// Yeni bir komut eklemek için:
//   1. Prototipi buraya ekle.
//   2. Uygun Cmd_*.cpp dosyasına implementasyonu yaz.
//   3. main_new.cpp içindeki AppServices nesnesine kaydet.
#pragma once

#include "neuronc/cli/PackageManager.h"
#include "neuronc/codegen/LLVMCodeGen.h"

#include <optional>
#include <string>

// ── Yardım mesajı ────────────────────────────────────────────────────────────
// İçerik UsageText.h'dan gelir — metni değiştirmek için oraya git.

void printUsage();
int cmdRepl();

// ── Paket yönetimi ───────────────────────────────────────────────────────────

int cmdPackages();
int cmdInstall();
int cmdAdd(const std::string &packageName,
           const neuron::PackageInstallOptions &options);
int cmdRemove(const std::string &packageName, bool removeGlobal);
int cmdUpdate(const std::optional<std::string> &packageName);
int cmdPublish(std::string *outArtifactPath = nullptr);
int cmdSettingsOf(const std::string &target);
int cmdDependenciesOf(const std::string &target);

// ── Proje oluşturma ──────────────────────────────────────────────────────────

int cmdNew(const std::string &name, bool library = false);

// ── Hata ayıklama komutları ──────────────────────────────────────────────────

int cmdLex(const std::string &filepath);
int cmdParse(const std::string &filepath);
int cmdNir(const std::string &filepath);

// ── Derleme komutları ────────────────────────────────────────────────────────

/// Tek .npp dosyasını derler.
int cmdCompile(const std::string &filepath,
               neuron::LLVMCodeGenOptions *outRuntimeOptions = nullptr);

int cmdBuildMinimal(int argc, char *argv[]);
int cmdBuildProduct(int argc, char *argv[]);
int cmdBuild();
int cmdBuildTarget(int argc, char *argv[]);
int cmdRun();
int cmdRunTarget(int argc, char *argv[]);
int cmdRelease();

// ── Surgeon (tanı & kurulum rehberi) ────────────────────────────────────────

int cmdSurgeon(int argc, char *argv[]);


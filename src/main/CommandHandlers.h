// CommandHandlers.h â€” CLI komut iÅŸleyicilerinin ortak arayÃ¼zÃ¼.
//
// Implementasyonlar 3 dosyaya bÃ¶lÃ¼nmÃ¼ÅŸtÃ¼r:
//   Cmd_Package.cpp  â†’ paket yÃ¶netimi + proje oluÅŸturma
//   Cmd_Debug.cpp    â†’ lex/parse/nir/compile + usage metni
//   Cmd_Build.cpp    â†’ build-nucleus / build-product / build / run / release
//
// YardÄ±m metni (usage) â†’ UsageText.h â€” SADECE orayÄ± dÃ¼zenle, buraya dokunma.
//
// Yeni bir komut eklemek iÃ§in:
//   1. Prototipi buraya ekle.
//   2. Uygun Cmd_*.cpp dosyasÄ±na implementasyonu yaz.
//   3. main_new.cpp iÃ§indeki AppServices nesnesine kaydet.
#pragma once

#include "neuronc/cli/PackageManager.h"
#include "neuronc/codegen/LLVMCodeGen.h"

#include <optional>
#include <string>

// â”€â”€ YardÄ±m mesajÄ± â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Ä°Ã§erik UsageText.h'dan gelir â€” metni deÄŸiÅŸtirmek iÃ§in oraya git.

void printUsage();
int cmdRepl();

// â”€â”€ Paket yÃ¶netimi â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

int cmdPackages();
int cmdInstall();
int cmdAdd(const std::string &packageName,
           const neuron::PackageInstallOptions &options);
int cmdRemove(const std::string &packageName, bool removeGlobal);
int cmdUpdate(const std::optional<std::string> &packageName);
int cmdPublish(std::string *outArtifactPath = nullptr);
int cmdSettingsOf(const std::string &target);
int cmdDependenciesOf(const std::string &target);

// â”€â”€ Proje oluÅŸturma â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

int cmdNew(const std::string &name, bool library = false);

// â”€â”€ Hata ayÄ±klama komutlarÄ± â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

int cmdLex(const std::string &filepath);
int cmdParse(const std::string &filepath);
int cmdNir(const std::string &filepath);

// â”€â”€ Derleme komutlarÄ± â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

/// Tek .nr dosyasÄ±nÄ± derler.
int cmdCompile(const std::string &filepath,
               neuron::LLVMCodeGenOptions *outRuntimeOptions = nullptr);

int cmdBuildMinimal(int argc, char *argv[]);
int cmdBuildProduct(int argc, char *argv[]);
int cmdBuild();
int cmdBuildTarget(int argc, char *argv[]);
int cmdRun();
int cmdRunTarget(int argc, char *argv[]);
int cmdRelease();

// â”€â”€ Surgeon (tanÄ± & kurulum rehberi) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

int cmdSurgeon(int argc, char *argv[]);


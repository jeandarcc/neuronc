// AppGlobals.h — Uygulama genelindeki global durum değişkenleri.
// Tüm main/ modülleri bu header'ı include eder; tanımlar AppGlobals.cpp'de
// yapılır. Yeni bir global eklemek istersen buraya ekle ve AppGlobals.cpp'de
// tanımını yap.
#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Derleyici araç zincirinin bin dizini (boşsa PATH üzerinden aranır).
extern std::string g_toolchainBinDir;

// Neuron++ araçlarının kök dizini (runtime, toolchain vb. buradan çözümlenir).
extern fs::path g_toolRoot;

// Derlenmiş runtime objelerinin önbellek dizini.
extern fs::path g_runtimeObjectDir;

// --trace-errors / NEURON_TRACE_ERRORS ile etkinleştirilen iz modu.
extern bool g_traceErrors;

// Tanı mesajlarında ANSI renklendirme etkin mi?
extern bool g_colorDiagnostics;

// --bypass-rules ile kural denetimi devre dışı mı?
extern bool g_bypassRules;


// AppGlobals.h â€” Uygulama genelindeki global durum deÄŸiÅŸkenleri.
// TÃ¼m main/ modÃ¼lleri bu header'Ä± include eder; tanÄ±mlar AppGlobals.cpp'de
// yapÄ±lÄ±r. Yeni bir global eklemek istersen buraya ekle ve AppGlobals.cpp'de
// tanÄ±mÄ±nÄ± yap.
#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Derleyici araÃ§ zincirinin bin dizini (boÅŸsa PATH Ã¼zerinden aranÄ±r).
extern std::string g_toolchainBinDir;

// Neuron araÃ§larÄ±nÄ±n kÃ¶k dizini (runtime, toolchain vb. buradan Ã§Ã¶zÃ¼mlenir).
extern fs::path g_toolRoot;

// DerlenmiÅŸ runtime objelerinin Ã¶nbellek dizini.
extern fs::path g_runtimeObjectDir;

// --trace-errors / NEURON_TRACE_ERRORS ile etkinleÅŸtirilen iz modu.
extern bool g_traceErrors;

// TanÄ± mesajlarÄ±nda ANSI renklendirme etkin mi?
extern bool g_colorDiagnostics;

// --bypass-rules ile kural denetimi devre dÄ±ÅŸÄ± mÄ±?
extern bool g_bypassRules;


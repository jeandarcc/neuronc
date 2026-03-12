// DiagnosticEngine.h — Tanı mesajı biçimlendirme ve raporlama motoru.
//
// Bu modül şu işlemleri sağlar:
//   - ANSI renklendirme (g_colorDiagnostics'e göre)
//   - Tanı konumu (dosya:satır:sütun) ayrıştırma
//   - Hata/uyarı kodları üretme (NPP1001, NPP2001 ...)
//   - Standart hata akışına biçimli tanı yazdırma
//   - Lexer/parser/semantic hata listeleri için toplu raporlama
//
// Yeni bir faz eklemek için diagnosticCodeForPhase() fonksiyonunu güncelle.
#pragma once

#include "neuronc/diagnostics/DiagnosticLocalizer.h"
#include "neuronc/diagnostics/DiagnosticLocale.h"
#include "neuronc/frontend/Diagnostics.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include <string>
#include <unordered_map>
#include <vector>

// ── Diagnostic severity ─────────────────────────────────────────────────────

enum class DiagnosticSeverity { Error, Warning };

// ── Location structure ──────────────────────────────────────────────────────

struct DiagnosticLocation {
  std::string file;
  int line = 0;
  int column = 0;
  std::string message;
  bool valid = false;
};

// ── Coloring ────────────────────────────────────────────────────────────────

/// Eğer g_colorDiagnostics aktifse metni ANSI kodu ile sarmalar.
std::string colorize(const std::string &text, const char *ansiColorCode);

// ── Diagnostic location parsing ──────────────────────────────────────────────

/// "dosya:satır:sütun: mesaj" biçimini ayrıştırır.
DiagnosticLocation parseDiagnosticLocation(const std::string &diagnostic);

// ── Code generation ─────────────────────────────────────────────────────────

/// Verilen faz ve şiddet için hata/uyarı kodu üretir (örn. "NPP1001").
std::string diagnosticCodeForPhase(const std::string &phase,
                                   DiagnosticSeverity severity);

// ── Message normalization ────────────────────────────────────────────────────

/// "semantic warning:", "error:" gibi önekleri ayıklar ve şiddeti belirler.
std::string normalizeDiagnosticMessage(const std::string &rawMessage,
                                       DiagnosticSeverity *outSeverity);

std::string currentDiagnosticLanguage();

// ── Printing ────────────────────────────────────────────────────────────────

/// Standart formatta tek bir tanıyı stderr'e yazar.
void printDiagnostic(const std::string &file, int line, int column,
                     DiagnosticSeverity severity, const std::string &code,
                     const std::string &message);

/// g_traceErrors aktifse kaynak satırını ve caret'i stderr'e yazar.
void printTraceLine(const std::string &source, const std::string &file,
                    int line, int column);

// ── Batch reporting ─────────────────────────────────────────────────────────

/// Lexer/parser string tanı listesini biçimleyerek stderr'e yazar.
void reportStringDiagnostics(const std::string &phase,
                             const std::string &filepath,
                             const std::string &source,
                             const std::vector<std::string> &diagnostics);

void reportFrontendDiagnostics(
    const std::vector<neuron::frontend::Diagnostic> &diagnostics,
    const std::unordered_map<std::string, std::string> &sourceByFile);

/// Semantic analyzer tanı listesini biçimleyerek stderr'e yazar.
void reportSemanticDiagnostics(
    const std::string &filepath, const std::string &source,
    const std::vector<neuron::SemanticError> &diagnostics);

/// .neuronsettings satırı için uyarı yazar.
void reportConfigWarning(const std::string &file, int line,
                         const std::string &message);

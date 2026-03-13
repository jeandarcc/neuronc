#include "lsp/DocumentManager.h"
#include "neuronc/diagnostics/DiagnosticLocalizer.h"
#include "neuronc/diagnostics/DiagnosticLocale.h"

#include <filesystem>

namespace {

std::filesystem::path getToolRoot() {
  return std::filesystem::absolute(std::filesystem::path(__FILE__))
      .parent_path()
      .parent_path()
      .parent_path();
}

neuron::frontend::Diagnostic makeDiagnostic(
    const std::string &code, const std::string &message,
    neuron::diagnostics::DiagnosticArguments arguments = {},
    neuron::frontend::DiagnosticSeverity severity =
        neuron::frontend::DiagnosticSeverity::Warning) {
  neuron::frontend::Diagnostic diagnostic;
  diagnostic.phase = "semantic";
  diagnostic.severity = severity;
  diagnostic.code = code;
  diagnostic.file = "demo.nr";
  diagnostic.range.start.line = 1;
  diagnostic.range.start.column = 1;
  diagnostic.range.end.line = 1;
  diagnostic.range.end.column = 2;
  diagnostic.message = message;
  diagnostic.arguments = std::move(arguments);
  return diagnostic;
}

} // namespace

TEST(DiagnosticLocalizerLoadsLocalizedCatalogEntries) {
  neuron::diagnostics::DiagnosticLocalizer localizer(
      getToolRoot());

  const auto trEntry = localizer.findEntry("tr", "NR9002");
  ASSERT_TRUE(trEntry.has_value());
  ASSERT_EQ(trEntry->defaultMessage, "Bildirilen baglama hic okunmuyor.");

  const auto enEntry = localizer.findEntry("en", "NR9002");
  ASSERT_TRUE(enEntry.has_value());
  ASSERT_EQ(enEntry->defaultMessage, "The declared binding is never read.");
  return true;
}

TEST(DocumentManagerLocalizesDiagnosticsUsingResolvedLanguage) {
  neuron::lsp::DocumentManager manager;
  manager.setToolRoot(getToolRoot());
  manager.setDiagnosticLanguage(neuron::diagnostics::ResolvedLanguage{
      .requested = "tr",
      .normalized = "tr",
      .effective = "tr",
      .mode = neuron::diagnostics::LanguageSettingMode::Explicit,
      .source = neuron::diagnostics::ResolvedLanguageSource::Explicit,
  });

  std::vector<neuron::frontend::Diagnostic> diagnostics = {
      makeDiagnostic("NR9002", "Unused variable: value")};
  neuron::diagnostics::DiagnosticLocalizer localizer(
      getToolRoot());
  const auto localized = localizer.localizeDiagnostics(diagnostics, "tr");

  ASSERT_EQ(localized.size(), static_cast<std::size_t>(1));
  ASSERT_TRUE(localized.front().message.find("Bildirilen baglama hic okunmuyor.") !=
              std::string::npos);
  ASSERT_TRUE(localized.front().message.find("NR9002") == std::string::npos);
  return true;
}

TEST(DiagnosticLocalizerRendersTemplateArgumentsFromCatalog) {
  neuron::diagnostics::DiagnosticLocalizer localizer(
      getToolRoot());
  const auto localized = localizer.localizeDiagnostics(
      {makeDiagnostic("N2204", "Variable is used before it is initialized: h",
                      {{"name", "h"}})},
      "tr");

  ASSERT_EQ(localized.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(localized.front().message,
            "Degisken baslatilmadan once kullaniliyor: h.");
  return true;
}

TEST(DiagnosticLocalizerRendersUnknownIdentifierTemplateArguments) {
  neuron::diagnostics::DiagnosticLocalizer localizer(
      getToolRoot());
  const auto localized = localizer.localizeDiagnostics(
      {makeDiagnostic("N2201", "Undefined identifier: f", {{"name", "f"}},
                      neuron::frontend::DiagnosticSeverity::Error)},
      "tr");

  ASSERT_EQ(localized.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(localized.front().message,
            "Basvurulan tanimlayici mevcut kapsamda tanimli degil: f.");
  return true;
}

TEST(DiagnosticLocalizerFallsBackToLocalizedGenericMessageWithoutRawText) {
  neuron::diagnostics::DiagnosticLocalizer localizer(
      getToolRoot());
  const auto localized = localizer.localizeDiagnostics(
      {makeDiagnostic("N2999", "Unknown internal compiler thing", {},
                      neuron::frontend::DiagnosticSeverity::Error)},
      "tr");

  ASSERT_EQ(localized.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(localized.front().code, "NR2001");
  ASSERT_EQ(localized.front().message,
            "Program sozdizimsel olarak gecerli ancak semantik olarak gecersiz.");
  return true;
}

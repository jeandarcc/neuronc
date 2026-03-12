#include "SettingsMacroInternal.h"

#include <utility>

namespace neuron::cli {

SettingsMacroProcessor::SettingsMacroProcessor(
    std::filesystem::path toolRoot, std::filesystem::path entrySourcePath)
    : m_toolRoot(normalizePath(toolRoot)),
      m_entrySourcePath(normalizePath(entrySourcePath)),
      m_impl(std::make_unique<Impl>()) {}

SettingsMacroProcessor::~SettingsMacroProcessor() = default;

} // namespace neuron::cli

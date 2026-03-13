#pragma once
#include <filesystem>
#include <string>

namespace neuron {

/// Generates a new Neuron project at the given path.
class ProjectGenerator {
public:
  /// Create a new project with the given name in the current directory.
  /// @param projectName  Name of the project (used as the directory name).
  /// @param toolRoot     Root of the toolchain installation (for template
  /// lookup).
  static bool createProject(const std::string &projectName,
                            const std::filesystem::path &toolRoot = {},
                            bool library = false);

private:
  static bool createDirectory(const std::string &path);
  static bool writeFile(const std::string &path, const std::string &content);
  static bool readFile(const std::filesystem::path &path,
                       std::string *outContent);
  static std::filesystem::path
  resolveTemplateDirectory(const std::filesystem::path &toolRoot);
  static std::filesystem::path
  resolveLearnNeuronDirectory(const std::filesystem::path &toolRoot,
                              const std::filesystem::path &templateDir);
  static bool copyTemplateFile(const std::filesystem::path &templatePath,
                               const std::string &destinationPath,
                               const std::string &fallbackContent);
  static bool copyDirectoryTree(const std::filesystem::path &sourcePath,
                                const std::filesystem::path &destinationPath);
};

} // namespace neuron

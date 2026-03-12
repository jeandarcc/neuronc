#include "neuronc/cli/ResourceCompiler.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace neuron {

namespace {

std::string quote(const std::string &s) { return "\"" + s + "\""; }

// Convert a version string like "1.2.3" to comma-separated "1,2,3,0" for
// Windows
std::string versionToWindowsFmt(const std::string &version, int build) {
  std::string result;
  int parts = 0;
  std::istringstream stream(version);
  std::string token;
  while (std::getline(stream, token, '.') && parts < 3) {
    if (!result.empty())
      result += ",";
    result += token;
    parts++;
  }
  while (parts < 3) {
    if (!result.empty())
      result += ",";
    result += "0";
    parts++;
  }
  result += "," + std::to_string(build);
  return result;
}

std::filesystem::path
compileWindowsResources(const ProductSettings &settings,
                        const std::filesystem::path &projectRoot,
                        const std::filesystem::path &outputDir, bool verbose) {
  namespace fs = std::filesystem;

  fs::path rcPath = outputDir / "resources.rc";
  fs::path objPath = outputDir / "resources.o";

  std::ofstream rc(rcPath);
  if (!rc.is_open()) {
    std::cerr << "Failed to create resource file: " << rcPath << std::endl;
    return {};
  }

  // 1. Icon
  if (!settings.iconWindows.empty()) {
    fs::path iconPath = projectRoot / settings.iconWindows;
    if (fs::exists(iconPath)) {
      // Must use double backslashes for path in .rc file
      std::string iconPathStr = iconPath.string();
      for (char &c : iconPathStr) {
        if (c == '\\')
          c = '/';
      }
      rc << "IDI_ICON1 ICON \"" << iconPathStr << "\"\n\n";
    } else {
      std::cerr << "Warning: Windows icon not found at " << iconPath
                << std::endl;
    }
  }

  // 2. Version Info
  std::string winVersion = versionToWindowsFmt(settings.productVersion,
                                               settings.productBuildVersion);

  rc << "1 VERSIONINFO\n"
     << "  FILEVERSION " << winVersion << "\n"
     << "  PRODUCTVERSION " << winVersion << "\n"
     << "BEGIN\n"
     << "  BLOCK \"StringFileInfo\"\n"
     << "  BEGIN\n"
     << "    BLOCK \"040904E4\"\n"
     << "    BEGIN\n"
     << "      VALUE \"CompanyName\", \"" << settings.productPublisher << "\"\n"
     << "      VALUE \"FileDescription\", \"" << settings.productDescription
     << "\"\n"
     << "      VALUE \"FileVersion\", \"" << settings.productVersion
     << " Build " << settings.productBuildVersion << "\"\n"
     << "      VALUE \"InternalName\", \"" << settings.outputName << "\"\n"
     << "      VALUE \"OriginalFilename\", \"" << settings.outputName
     << ".exe\"\n"
     << "      VALUE \"ProductName\", \"" << settings.productName << "\"\n"
     << "      VALUE \"ProductVersion\", \"" << settings.productVersion
     << "\"\n"
     << "    END\n"
     << "  END\n"
     << "  BLOCK \"VarFileInfo\"\n"
     << "  BEGIN\n"
     << "    VALUE \"Translation\", 0x409, 1252\n"
     << "  END\n"
     << "END\n";

  rc.close();

  // 3. Compile with windres
  std::string cmd =
      "windres -i " + quote(rcPath.string()) + " -o " + quote(objPath.string());
  if (verbose) {
    std::cout << "[build-product] " << cmd << std::endl;
  }
  int ret = std::system(cmd.c_str());
  if (ret != 0) {
    std::cerr << "windres execution failed (code " << ret << ")\n"
              << "Make sure windres is in your PATH." << std::endl;
    return {};
  }

  return objPath;
}

} // namespace

std::filesystem::path
runResourceCompiler(const ProductSettings &settings,
                    const std::filesystem::path &projectRoot,
                    const std::filesystem::path &outputDir,
                    const std::string &platformId, bool verbose) {
  if (platformId == "windows" || platformId == "windows-x64") {
    return compileWindowsResources(settings, projectRoot, outputDir, verbose);
  }
  // Linux and macOS resource compilation to be implemented in the future.
  // For now, return empty path (no object file to link).
  return {};
}

} // namespace neuron

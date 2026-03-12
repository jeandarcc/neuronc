#include "neuronc/cli/ProjectGenerator.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace neuron {

bool ProjectGenerator::createDirectory(const std::string &path) {
  std::error_code ec;
  fs::create_directories(path, ec);
  if (ec) {
    std::cerr << "Error creating directory '" << path << "': " << ec.message()
              << std::endl;
    return false;
  }
  return true;
}

bool ProjectGenerator::writeFile(const std::string &path,
                                 const std::string &content) {
  std::ofstream file(path);
  if (!file.is_open()) {
    std::cerr << "Error creating file '" << path << "'" << std::endl;
    return false;
  }
  file << content;
  return true;
}

bool ProjectGenerator::readFile(const fs::path &path, std::string *outContent) {
  if (outContent == nullptr) {
    return false;
  }
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  *outContent = ss.str();
  return true;
}

fs::path
ProjectGenerator::resolveTemplateDirectory(const fs::path &toolRoot) {
  std::vector<fs::path> candidates;
  if (!toolRoot.empty()) {
    candidates.push_back(toolRoot / "src" / "cli" / "templates");
    candidates.push_back(toolRoot / "templates");
  }

  fs::path probe = fs::current_path();
  for (int depth = 0; depth < 6; ++depth) {
    candidates.push_back(probe / "src" / "cli" / "templates");
    candidates.push_back(probe / "templates");
    const fs::path parent = probe.parent_path();
    if (parent == probe) {
      break;
    }
    probe = parent;
  }

  for (const fs::path &candidate : candidates) {
    if (!candidate.empty() && fs::exists(candidate)) {
      return candidate;
    }
  }

  return {};
}

fs::path
ProjectGenerator::resolveLearnNeuronDirectory(const fs::path &toolRoot,
                                              const fs::path &templateDir) {
  std::vector<fs::path> candidates;
  if (!templateDir.empty()) {
    candidates.push_back(templateDir / "agents" / "learnneuron");
  }
  if (!toolRoot.empty()) {
    candidates.push_back(toolRoot / "docs" / "learnneuron");
  }

  fs::path probe = fs::current_path();
  for (int depth = 0; depth < 6; ++depth) {
    candidates.push_back(probe / "docs" / "learnneuron");
    const fs::path parent = probe.parent_path();
    if (parent == probe) {
      break;
    }
    probe = parent;
  }

  for (const fs::path &candidate : candidates) {
    if (!candidate.empty() && fs::exists(candidate)) {
      return candidate;
    }
  }

  return {};
}

bool ProjectGenerator::copyTemplateFile(const fs::path &templatePath,
                                        const std::string &destinationPath,
                                        const std::string &fallbackContent) {
  std::string content;
  if (!templatePath.empty() && fs::exists(templatePath) &&
      readFile(templatePath, &content)) {
    return writeFile(destinationPath, content);
  }
  return writeFile(destinationPath, fallbackContent);
}

bool ProjectGenerator::copyDirectoryTree(const fs::path &sourcePath,
                                         const fs::path &destinationPath) {
  std::error_code ec;
  if (sourcePath.empty() || !fs::exists(sourcePath)) {
    return false;
  }

  fs::create_directories(destinationPath, ec);
  if (ec) {
    std::cerr << "Error creating directory '" << destinationPath.string()
              << "': " << ec.message() << std::endl;
    return false;
  }

  for (fs::recursive_directory_iterator it(sourcePath, ec), end;
       it != end && !ec; it.increment(ec)) {
    const fs::path relative = fs::relative(it->path(), sourcePath, ec);
    if (ec) {
      std::cerr << "Error computing relative path for '"
                << it->path().string() << "': " << ec.message() << std::endl;
      return false;
    }
    const fs::path destination = destinationPath / relative;
    if (it->is_directory()) {
      fs::create_directories(destination, ec);
      if (ec) {
        std::cerr << "Error creating directory '" << destination.string()
                  << "': " << ec.message() << std::endl;
        return false;
      }
      continue;
    }
    fs::create_directories(destination.parent_path(), ec);
    if (ec) {
      std::cerr << "Error creating directory '" << destination.parent_path().string()
                << "': " << ec.message() << std::endl;
      return false;
    }
    fs::copy_file(it->path(), destination, fs::copy_options::overwrite_existing,
                  ec);
    if (ec) {
      std::cerr << "Error copying '" << it->path().string() << "' to '"
                << destination.string() << "': " << ec.message() << std::endl;
      return false;
    }
  }

  return !ec;
}

bool ProjectGenerator::createProject(const std::string &projectName,
                                     const fs::path &toolRoot,
                                     bool library) {
  std::string projectDir = projectName;

  const fs::path templateDir = resolveTemplateDirectory(toolRoot);
  const fs::path learnNeuronDir =
      resolveLearnNeuronDirectory(toolRoot, templateDir);

  // Create directory structure
  if (!createDirectory(projectDir))
    return false;
  if (!createDirectory(projectDir + "/src"))
    return false;
  if (!createDirectory(projectDir + "/modules"))
    return false;
  if (!createDirectory(projectDir + "/build"))
    return false;
  if (!createDirectory(projectDir + "/docs/scripts"))
    return false;
  if (!createDirectory(projectDir + "/tests/auto"))
    return false;
  if (!createDirectory(projectDir + "/tests/unit"))
    return false;
  if (!createDirectory(projectDir + "/agents/language/Details"))
    return false;
  if (!createDirectory(projectDir + "/agents/learnneuron"))
    return false;
  if (!createDirectory(projectDir + "/agents/project"))
    return false;
  if (!createDirectory(projectDir + "/assets"))
    return false;

  // neuron.toml
  std::string toml = "[project]\n"
                     "name = \"" +
                     projectName +
                     "\"\n"
                     "version = \"0.1.0\"\n"
                     "\n"
                     "[package]\n"
                     "kind = \"" +
                     std::string(library ? "library" : "application") +
                     "\"\n"
                     "description = \"" +
                     std::string(library ? "Neuron library package"
                                         : "Neuron application project") +
                     "\"\n"
                     "repository = \"https://github.com/your-org/" +
                     projectName +
                     "\"\n"
                     "license = \"MIT\"\n"
                     "source_dir = \"src\"\n"
                     "\n"
                     "[build]\n"
                     "main = \"src/Main.npp\"\n"
                     "build_dir = \"build\"\n"
                     "optimize = \"aggressive\"\n"
                     "emit_ir = \"optimized\"\n"
                     "target_cpu = \"native\"\n"
                     "tensor_profile = \"balanced\"\n"
                     "tensor_autotune = true\n"
                     "tensor_kernel_cache = \"build/.neuron_cache/tensor/\"\n"
                     "\n"
                     "[web]\n"
                     "canvas_id = \"neuron-canvas\"\n"
                     "wgsl_cache = true\n"
                     "dev_server_port = 8080\n"
                     "enable_shared_array = true\n"
                     "initial_memory_mb = 64\n"
                     "maximum_memory_mb = 512\n"
                     "wasm_simd = true\n"
                     "\n"
                     "[dependencies]\n";
  if (!writeFile(projectDir + "/neuron.toml", toml))
    return false;

  // .neuronsettings
  std::string settings = "# Neuron++ source rules\n"
                         "max_classes_per_file = 1\n"
                         "max_lines_per_file = 1000\n"
                         "require_method_uppercase_start = true\n"
                         "enforce_strict_file_naming = true\n"
                         "forbid_root_scripts = true\n"
                         "max_lines_per_method = 50\n"
                         "max_lines_per_block_statement = 20\n"
                         "min_method_name_length = 4\n"
                         "require_class_explicit_visibility = true\n"
                         "require_property_explicit_visibility = true\n"
                         "require_const_uppercase = true\n"
                         "max_nesting_depth = 3\n"
                         "require_script_docs = true\n"
                         "require_script_docs_exclude = [\"Test*\"]\n"
                         "require_script_docs_min_lines = 5\n"
                         "max_auto_test_duration_ms = 5000\n"
                         "require_public_method_docs = true\n"
                         "package_auto_add_missing = true\n"
                         "agent_hints = true\n";
  if (!writeFile(projectDir + "/.neuronsettings", settings))
    return false;

  // .productsettings
  std::string productSettings =
      "product_name = \"" + projectName +
      "\"\n"
      "product_version = \"1.0.0\"\n"
      "product_build_version = 1\n"
      "product_publisher = \"\"\n"
      "product_description = \"A Neuron++ application\"\n"
      "product_website = \"\"\n\n"
      "icon_windows = \"\"\n"
      "icon_linux = \"\"\n"
      "icon_macos = \"\"\n"
      "splash_image = \"\"\n\n"
      "output_name = \"" +
      projectName +
      "\"\n"
      "output_dir = \"build/product\"\n\n"
      "installer_enabled = true\n"
      "installer_style = \"modern\"\n"
      "installer_license_file = \"\"\n"
      "installer_banner_image = \"\"\n"
      "installer_accent_color = \"#0078D4\"\n"
      "install_directory_default = \"%PROGRAMFILES%\\\\{product_name}\"\n"
      "create_desktop_shortcut = true\n"
      "create_start_menu_entry = true\n"
      "file_associations = []\n\n"
      "update_enabled = false\n"
      "update_url = \"\"\n"
      "update_check_interval_hours = 24\n"
      "update_channel = \"stable\"\n"
      "update_public_key = \"\"\n\n"
      "uninstaller_enabled = true\n"
      "uninstaller_name = \"Uninstall {product_name}\"\n";
  if (!writeFile(projectDir + "/.productsettings", productSettings))
    return false;

  // .gitignore
  std::string gitignore = "build/\n"
                          "Build/\n"
                          "modules/\n"
                          "agents/\n";
  if (!writeFile(projectDir + "/.gitignore", gitignore))
    return false;

  // Main.npp
  std::string mainNpp;
  if (library) {
    mainNpp = "// " + projectName + " - Neuron++ Library\n"
              "\n"
              "ComputeGreeting is method() -> string\n"
              "{\n"
              "    return \"Hello from " + projectName + "\";\n"
              "};\n";
  } else {
    mainNpp = "// " + projectName +
              " - Neuron++ Project\n"
              "\n"
              "Init is method()\n"
              "{\n"
              "    Print(\"Hello Neuron!\");\n"
              "};\n";
  }
  if (!writeFile(projectDir + "/src/Main.npp", mainNpp))
    return false;

  std::string readme = "# " + projectName + "\n\n";
  if (library) {
    readme += "Neuron library package scaffold.\n";
  } else {
    readme += "Neuron application scaffold.\n";
  }
  if (!writeFile(projectDir + "/README.md", readme))
    return false;

  if (!writeFile(projectDir + "/LICENSE", "MIT\n"))
    return false;

  // docs/scripts/Main.md — from template
  if (!copyTemplateFile(
          templateDir / "docs" / "scripts" / "Main.md",
          projectDir + "/docs/scripts/Main.md",
          "# Main Script\n\n"
          "This document describes `src/Main.npp`.\n\n"
          "## Purpose\n"
          "Entry point for the Neuron++ project. Runs the `Init` method on "
          "startup.\n\n"
          "## Usage\n"
          "The `Init` method is automatically invoked by the runtime when the "
          "project\n"
          "is executed via `neuron run`. No manual call is required.\n"))
    return false;

  // agents/language/LanguageGuide.md — from template
  if (!copyTemplateFile(
          templateDir / "agents" / "language" / "LanguageGuide.md",
          projectDir + "/agents/language/LanguageGuide.md",
          "# Language Guide\n\nProject language conventions and examples.\n"))
    return false;

  // agents/language/Details/*.md — from templates
  if (!copyTemplateFile(
          templateDir / "agents" / "language" / "Details" / "RULES.md",
          projectDir + "/agents/language/Details/RULES.md",
          "# Rules\n\nCore language rules for contributors and agents.\n"))
    return false;

  if (!copyTemplateFile(
          templateDir / "agents" / "language" / "Details" / "ERROR_GUIDE.md",
          projectDir + "/agents/language/Details/ERROR_GUIDE.md",
          "# Error Guide\n\nFrequent diagnostics and how to fix them.\n"))
    return false;

  if (!copyTemplateFile(templateDir / "agents" / "language" / "Details" /
                            "NAMING.md",
                        projectDir + "/agents/language/Details/NAMING.md",
                        "# Naming\n\nNaming standards for methods, classes, "
                        "and constants.\n"))
    return false;

  if (!copyTemplateFile(
          templateDir / "agents" / "language" / "Details" / "GPU_SEMANTICS.md",
          projectDir + "/agents/language/Details/GPU_SEMANTICS.md",
          "# GPU Semantics\n\nGPU block usage and execution guidance.\n"))
    return false;

  if (!copyTemplateFile(
          templateDir / "agents" / "language" / "Details" / "STRUCTURE.md",
          projectDir + "/agents/language/Details/STRUCTURE.md",
          "# Structure\n\nRecommended project and script structure.\n"))
    return false;

  // agents/project/*.md — from templates
  if (!copyTemplateFile(templateDir / "agents" / "project" / "ARCHITECTURE.md",
                        projectDir + "/agents/project/ARCHITECTURE.md",
                        "# Architecture\n\nHigh-level architecture notes for "
                        "this project.\n"))
    return false;

  if (!copyTemplateFile(
          templateDir / "agents" / "project" / "RULES.md",
          projectDir + "/agents/project/RULES.md",
          "# Project Rules\n\nProject-specific constraints and workflows.\n"))
    return false;

  if (!learnNeuronDir.empty() && fs::exists(learnNeuronDir)) {
    if (!copyDirectoryTree(learnNeuronDir,
                           fs::path(projectDir) / "agents" / "learnneuron")) {
      return false;
    }
  } else if (!writeFile(projectDir + "/agents/learnneuron/README.md",
                        "# Learn Neuron\n\nReference docs were not found in "
                        "the installed toolchain.\n")) {
    return false;
  }

  std::cout << "Created new Neuron++ project: " << projectName << std::endl;
  std::cout << std::endl;
  std::cout << "  " << projectName << "/" << std::endl;
  std::cout << "  |- neuron.toml" << std::endl;
  std::cout << "  |- .neuronsettings" << std::endl;
  std::cout << "  |- .productsettings" << std::endl;
  std::cout << "  |- .gitignore" << std::endl;
  std::cout << "  |- assets/" << std::endl;
  std::cout << "  |- src/" << std::endl;
  std::cout << "  |  |- Main.npp" << std::endl;
  std::cout << "  |- docs/" << std::endl;
  std::cout << "  |  |- scripts/" << std::endl;
  std::cout << "  |     |- Main.md" << std::endl;
  std::cout << "  |- tests/" << std::endl;
  std::cout << "  |  |- auto/" << std::endl;
  std::cout << "  |  |- unit/" << std::endl;
  std::cout << "  |- agents/" << std::endl;
  std::cout << "  |  |- language/" << std::endl;
  std::cout << "  |  |  |- LanguageGuide.md" << std::endl;
  std::cout << "  |  |  |- Details/" << std::endl;
  std::cout << "  |  |     |- RULES.md" << std::endl;
  std::cout << "  |  |     |- ERROR_GUIDE.md" << std::endl;
  std::cout << "  |  |     |- NAMING.md" << std::endl;
  std::cout << "  |  |     |- GPU_SEMANTICS.md" << std::endl;
  std::cout << "  |  |     |- STRUCTURE.md" << std::endl;
  std::cout << "  |  |- learnneuron/" << std::endl;
  std::cout << "  |  |- project/" << std::endl;
  std::cout << "  |     |- ARCHITECTURE.md" << std::endl;
  std::cout << "  |     |- RULES.md" << std::endl;
  std::cout << "  |- modules/" << std::endl;
  std::cout << "  |- build/" << std::endl;
  std::cout << std::endl;
  std::cout << "Get started:" << std::endl;
  std::cout << "  cd " << projectName << std::endl;
  std::cout << "  neuron run" << std::endl;
  std::cout << "  neuron build-product --platform Windows" << std::endl;

  return true;
}

} // namespace neuron

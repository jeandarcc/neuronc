// UsageText.h — Neuron++ CLI help text.
//
// printUsage() writes this constant string to stdout.
// When a new command is added, it's enough to ONLY update this file;
// no need to touch CommandHandlers.cpp.
//
// Formatting rules:
//   - Each command line must be indented with two spaces
//   - Long descriptions must be aligned with a tab + space
//   - Maintain existing layout when adding a new command block
#pragma once

namespace neuron::cli {

inline constexpr const char *kUsageText = R"(
  _   _                              _     _
  | \ | | ___ _   _ _ __ ___  _ __  _| |_ _| |_
  |  \| |/ _ \ | | | '__/ _ \| '_ \(_   _|_   _)
  | |\  |  __/ |_| | | | (_) | | | | |_|   |_|
  |_| \_|\___|\__,_|_|  \___/|_| |_|

  Neuron++ Compiler v0.1.0

  Usage:
    neuron                       Start interactive REPL when stdin is a TTY
    neuron --language            Show current diagnostic language and how to change it
    neuron --language <code>     Persist a diagnostic language (example: tr, en, ja)
    neuron --language auto       Follow OS language when available, else fallback to English
    neuron new <project-name> [--lib]
                                 Create a new Neuron++ project or library
    neuron init <project-name> [--lib]
                                 Create a new Neuron++ project or library
    neuron add <owner/repo|url> [--version X] [--tag T] [--commit SHA] [--global]
                                 Add a package dependency or install globally
    neuron install               Install local dependencies from neuron.toml/neuron.lock
    neuron remove <package> [--global]
                                 Remove a package dependency or global package
    neuron update [package]      Update one/all dependencies
    neuron publish               Create a .nrkg package artifact
    neuron packages              List installed local/global packages
    neuron settings-of <builtin-module|package-name|owner/repo|url>
                                 Print effective .modulesettings content for a builtin or installed package
    neuron dependencies-of <builtin-module|package-name|owner/repo|url>
                                 Print dependency metadata for a builtin or installed package
    neuron release               Build/test/package release bundle
    neuron build                 Build the current project
    neuron build --target=web    Build web artifacts into build/web
    neuron run                   Build and run the current project
    neuron run --target=web      Serve build/web with embedded COOP/COEP dev server
    neuron build-nucleus --platform <Windows|Linux|Mac> [--compiler <path>] [--output <path>] [--verbose]
                                 Build single-file nucleus runtime for .ncon execution
    neuron ncon <command>        Build/run/inspect NCON containers
    neuron lex <file> [--trace-errors] [--bypass-rules]
                                 Tokenize a .nr file (debug)
    neuron parse <file> [--trace-errors] [--bypass-rules]
                                 Parse a .nr file (debug)
    neuron nir <file> [--trace-errors] [--bypass-rules]
                                 Generate and print NIR for a .nr file (debug)
    neuron compile <file> [--trace-errors] [--bypass-rules]
                                 Compile a .nr file to native executable
    neuron help                  Show this help message

  REPL:
    Enter                        Submit current buffer
    Ctrl+Enter                   Insert newline without submitting (Windows console)
    :run                         Submit buffered multi-line input
    :reset                       Clear REPL session state
    :clear                       Clear the console
    :quit                        Exit the REPL

  Tracing:
    --trace-errors               Print source-context traces for diagnostics
    --bypass-rules               Disable .neuronsettings rule enforcement for this run
    NEURON_TRACE_ERRORS=1        Enable tracing via environment variable
    NEURON_COLOR=0|1             Disable/enable diagnostic colors (NO_COLOR also works)
    NEURON_TOOLCHAIN_BIN=...     Override bundled gcc/binutils directory

 )";

} // namespace neuron::cli

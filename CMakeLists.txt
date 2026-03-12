cmake_minimum_required(VERSION 3.20)
project(NeuronPP VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(NPP_TOOLCHAIN_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

option(NPP_ENABLE_VULKAN_BACKEND "Enable Vulkan compute backend support" ON)
option(NPP_ENABLE_CUDA_BACKEND "Enable CUDA driver backend support" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(NOT DEFINED LLVM_DIR OR LLVM_DIR STREQUAL "")
    if(DEFINED ENV{LLVM_DIR} AND NOT "$ENV{LLVM_DIR}" STREQUAL "")
        set(LLVM_DIR "$ENV{LLVM_DIR}" CACHE PATH "Path to LLVMConfig.cmake directory")
    elseif(WIN32)
        set(_NPP_LLVM_CMAKE_CANDIDATES
            "C:/Program Files/LLVM/lib/cmake/llvm"
            "C:/Program Files (x86)/LLVM/lib/cmake/llvm"
            "C:/LLVM/lib/cmake/llvm"
        )
        foreach(_cand IN LISTS _NPP_LLVM_CMAKE_CANDIDATES)
            if(EXISTS "${_cand}/LLVMConfig.cmake")
                set(LLVM_DIR "${_cand}" CACHE PATH "Path to LLVMConfig.cmake directory")
                break()
            endif()
        endforeach()
        unset(_NPP_LLVM_CMAKE_CANDIDATES)
    endif()
endif()

find_package(LLVM CONFIG QUIET)
if(NOT LLVM_FOUND)
    message(FATAL_ERROR
        "LLVM not found. Configure with "
        "-DLLVM_DIR=\"<llvm-root>/lib/cmake/llvm\" "
        "(example: -DLLVM_DIR=\"C:/Program Files/LLVM/lib/cmake/llvm\").")
endif()
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
if(EMSCRIPTEN)
    set(NPP_FFI_INCLUDE_DIR "" CACHE PATH "Path to ffi.h (unused for emscripten)")
    set(NPP_FFI_LIBRARY "" CACHE FILEPATH "Path to libffi library (unused for emscripten)")
else()
    find_path(NPP_FFI_INCLUDE_DIR NAMES ffi.h)
    find_library(NPP_FFI_LIBRARY NAMES ffi libffi)
    if(NOT NPP_FFI_INCLUDE_DIR OR NOT NPP_FFI_LIBRARY)
        message(FATAL_ERROR "libffi development files not found")
    endif()
endif()

llvm_map_components_to_libnames(LLVM_LIBS
    core
    executionengine
    jitlink
    orcjit
    orcshared
    support
    irreader
    native
    nativecodegen
    passes
    x86asmparser
    x86codegen
    x86desc
    x86info
    webassemblyasmparser
    webassemblycodegen
    webassemblydesc
    webassemblyinfo
)

set(NPP_LLVM_DEFINES)
foreach(LLVM_DEF IN LISTS LLVM_DEFINITIONS)
    set(LLVM_DEF_ITEMS)
    separate_arguments(LLVM_DEF_ITEMS UNIX_COMMAND "${LLVM_DEF}")
    if(LLVM_DEF_ITEMS STREQUAL "")
        set(LLVM_DEF_ITEMS "${LLVM_DEF}")
    endif()

    foreach(LLVM_DEF_ITEM IN LISTS LLVM_DEF_ITEMS)
        string(REGEX REPLACE "^-D" "" LLVM_DEF_STRIPPED "${LLVM_DEF_ITEM}")
        if(LLVM_DEF_STRIPPED STREQUAL "")
            continue()
        endif()
        if(LLVM_DEF_STRIPPED MATCHES "^_FILE_OFFSET_BITS=.*")
            set(LLVM_DEF_STRIPPED "_FILE_OFFSET_BITS=64")
        endif()
        list(APPEND NPP_LLVM_DEFINES "${LLVM_DEF_STRIPPED}")
    endforeach()
endforeach()
list(REMOVE_DUPLICATES NPP_LLVM_DEFINES)

add_subdirectory(src/lexer)
add_subdirectory(src/parser)
add_subdirectory(src/sema)
add_subdirectory(src/frontend)
add_subdirectory(src/nir)
add_subdirectory(src/mir)
add_subdirectory(src/codegen)
add_subdirectory(src/cli)

add_library(libneuron STATIC
    $<TARGET_OBJECTS:neuronc_lexer>
    $<TARGET_OBJECTS:neuronc_parser>
    $<TARGET_OBJECTS:neuronc_sema>
    $<TARGET_OBJECTS:neuronc_frontend>
)
set_target_properties(libneuron PROPERTIES OUTPUT_NAME neuron)
target_include_directories(libneuron PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(neuronc STATIC
    $<TARGET_OBJECTS:neuronc_nir>
    $<TARGET_OBJECTS:neuronc_mir>
    $<TARGET_OBJECTS:neuronc_codegen>
    $<TARGET_OBJECTS:neuronc_cli>
)
target_include_directories(neuronc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LLVM_INCLUDE_DIRS}
)
target_compile_definitions(neuronc PUBLIC ${NPP_LLVM_DEFINES})
target_link_libraries(neuronc PUBLIC libneuron ${LLVM_LIBS})

add_subdirectory(runtime)

add_subdirectory(src/ncon)

add_executable(neuron
    src/main.cpp
    src/main/AppGlobals.cpp
    src/main/ToolchainUtils.cpp
    src/main/RuntimePaths.cpp
    src/main/DiagnosticEngine.cpp
    src/main/SettingsLoader.cpp
    src/main/UserProfileSettings.cpp
    src/main/ProjectHelpers.cpp
    src/main/BuildSupport.cpp
    src/main/UsageTextBuilder.cpp
    src/diagnostics/DiagnosticLocale.cpp
    src/diagnostics/DiagnosticLocalizer.cpp
    # Komut işleyiciler — her grup kendi dosyasında (bkz. CommandHandlers.h)
    src/main/Cmd_Package.cpp   # cmdNew, cmdPackages, cmdAdd/Remove/Update/Publish
    src/main/Cmd_Debug.cpp     # printUsage(→UsageText.h), cmdLex/Parse/Nir/Compile
    src/main/Cmd_Repl.cpp
    src/main/Cmd_Build.cpp     # cmdBuildMinimal/Product, cmdBuild/Run/Release
    src/main/Cmd_Surgeon.cpp   # cmdSurgeon
)
target_include_directories(neuron PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(neuron PRIVATE neuronc nconlib)
target_compile_definitions(neuron PRIVATE NPP_TOOLCHAIN_BIN_DIR="${NPP_TOOLCHAIN_BIN_DIR}")

add_executable(neuron-lsp
    src/neuron_lsp_main.cpp
    src/diagnostics/DiagnosticLocale.cpp
    src/diagnostics/DiagnosticLocalizer.cpp
    src/main/UserProfileSettings.cpp
    src/lsp/CallHierarchyHandler.cpp
    src/lsp/CodeActionHandler.cpp
    src/lsp/CompletionHandler.cpp
    src/lsp/DebugViewHandler.cpp
    src/lsp/DefinitionHandler.cpp
    src/lsp/DiagnosticsHandler.cpp
    src/lsp/DocumentManager.cpp
    src/lsp/HoverHandler.cpp
    src/lsp/LspAst.cpp
    src/lsp/LspCompletionSupport.cpp
    src/lsp/LspDebugViews.cpp
    src/lsp/LspDispatcher.cpp
    src/lsp/LspDocumentQueries.cpp
    src/lsp/LspFeaturePayloads.cpp
    src/lsp/LspHoverSupport.cpp
    src/lsp/LspPath.cpp
    src/lsp/LspProtocol.cpp
    src/lsp/LspSymbols.cpp
    src/lsp/LspText.cpp
    src/lsp/LspTransport.cpp
    src/lsp/LspWorkspace.cpp
    src/lsp/NeuronLspServer.cpp
    src/lsp/RenameHandler.cpp
    src/lsp/SymbolsInlayHints.cpp
    src/lsp/SymbolsHandler.cpp
    src/lsp/TypeHierarchyHandler.cpp
    src/lsp/WorkspaceSymbolsHandler.cpp
)
target_include_directories(neuron-lsp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LLVM_INCLUDE_DIRS}
)
target_link_libraries(neuron-lsp PRIVATE neuronc)

add_executable(ncon src/ncon_main.cpp)
target_link_libraries(ncon PRIVATE nconlib)

add_executable(nconmini src/ncon_mini_main.cpp)
target_link_libraries(nconmini PRIVATE nconlib)

enable_testing()
add_executable(neuron_tests
    tests/test_main.cpp
    src/diagnostics/DiagnosticLocale.cpp
    src/diagnostics/DiagnosticLocalizer.cpp
    src/main/AppGlobals.cpp
    src/main/DiagnosticEngine.cpp
    src/main/UserProfileSettings.cpp
    src/main/UsageTextBuilder.cpp
    src/lsp/DocumentManager.cpp
    src/lsp/LspAst.cpp
    src/lsp/LspDebugViews.cpp
    src/lsp/LspDispatcher.cpp
    src/lsp/LspDocumentQueries.cpp
    src/lsp/LspPath.cpp
    src/lsp/LspProtocol.cpp
    src/lsp/LspText.cpp
    src/lsp/LspTransport.cpp
    src/lsp/LspWorkspace.cpp
)
target_link_libraries(neuron_tests PRIVATE neuronc nconlib)
target_include_directories(neuron_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
)
add_test(NAME NeuronTests COMMAND neuron_tests)

if(WIN32)
    set(NPP_RUNTIME_DLLS
        "${NPP_TOOLCHAIN_BIN_DIR}/libstdc++-6.dll"
        "${NPP_TOOLCHAIN_BIN_DIR}/libgcc_s_seh-1.dll"
        "${NPP_TOOLCHAIN_BIN_DIR}/libwinpthread-1.dll"
        "${NPP_TOOLCHAIN_BIN_DIR}/zlib1.dll"
        "${NPP_TOOLCHAIN_BIN_DIR}/libzstd.dll"
    )
    file(GLOB NPP_LLVM_DLLS "${NPP_TOOLCHAIN_BIN_DIR}/libLLVM*.dll")
    file(GLOB NPP_LIBFFI_DLLS "${NPP_TOOLCHAIN_BIN_DIR}/libffi-*.dll")
    file(GLOB NPP_LIBXML2_DLLS "${NPP_TOOLCHAIN_BIN_DIR}/libxml2-*.dll")
    list(APPEND NPP_RUNTIME_DLLS ${NPP_LLVM_DLLS})
    list(APPEND NPP_RUNTIME_DLLS ${NPP_LIBFFI_DLLS})
    list(APPEND NPP_RUNTIME_DLLS ${NPP_LIBXML2_DLLS})

    add_custom_command(TARGET neuron POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${NPP_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:neuron>
        COMMAND_EXPAND_LISTS
    )
    add_custom_command(TARGET ncon POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${NPP_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:ncon>
        COMMAND_EXPAND_LISTS
    )
    add_custom_command(TARGET neuron-lsp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${NPP_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:neuron-lsp>
        COMMAND_EXPAND_LISTS
    )
    add_custom_command(TARGET nconmini POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${NPP_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:nconmini>
        COMMAND_EXPAND_LISTS
    )
    add_custom_command(TARGET neuron_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${NPP_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:neuron_tests>
        COMMAND_EXPAND_LISTS
    )
endif()

if(WIN32)
    set(NPP_INSTALLER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/installer/windows/Build-Installer.ps1")
    find_program(NPP_POWERSHELL_EXECUTABLE NAMES pwsh powershell)

    if(NPP_POWERSHELL_EXECUTABLE AND EXISTS "${NPP_INSTALLER_SCRIPT}")
        add_custom_target(installer_stage
            COMMAND "${NPP_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${NPP_INSTALLER_SCRIPT}"
                -BuildDir "${CMAKE_BINARY_DIR}"
                -StageDir "${CMAKE_BINARY_DIR}/installer/stage"
                -OutputDir "${CMAKE_BINARY_DIR}/installer/out"
                -ReleaseDir "${CMAKE_BINARY_DIR}/installer/release"
                -SkipBuild
                -SkipInstaller
                -SkipReleaseArtifacts
            USES_TERMINAL
            COMMENT "Preparing Neuron++ installer staging artifacts"
        )
        add_dependencies(installer_stage neuron ncon)

        add_custom_target(installer_bundle
            COMMAND "${NPP_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${NPP_INSTALLER_SCRIPT}"
                -BuildDir "${CMAKE_BINARY_DIR}"
                -StageDir "${CMAKE_BINARY_DIR}/installer/stage"
                -OutputDir "${CMAKE_BINARY_DIR}/installer/out"
                -ReleaseDir "${CMAKE_BINARY_DIR}/installer/release"
                -SkipBuild
            USES_TERMINAL
            COMMENT "Building Neuron++ installer and release artifacts"
        )
        add_dependencies(installer_bundle neuron ncon)
    else()
        message(WARNING "Installer targets were not created (PowerShell or installer script missing).")
    endif()
endif()

set(NPP_HELP_TOML_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/config/cli/help.toml")
add_custom_command(TARGET neuron POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:neuron>/config/cli
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${NPP_HELP_TOML_SOURCE}"
        $<TARGET_FILE_DIR:neuron>/config/cli/help.toml
)
add_custom_command(TARGET neuron_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:neuron_tests>/config/cli
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${NPP_HELP_TOML_SOURCE}"
        $<TARGET_FILE_DIR:neuron_tests>/config/cli/help.toml
)

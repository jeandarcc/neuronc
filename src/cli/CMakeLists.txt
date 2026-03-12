add_library(neuronc_cli OBJECT
    CommandDispatcher.cpp
    commands/DebugCommands.cpp
    commands/PackageQueryCommands.cpp
    commands/ProjectCommands.cpp
    pipeline/CompilePipeline.cpp
    repl/ReplConsoleReader.cpp
    repl/ReplPipeline.cpp
    SettingsMacroContext.cpp
    SettingsMacroEffective.cpp
    SettingsMacroExpansion.cpp
    SettingsMacroParsingCursor.cpp
    SettingsMacroParsing.cpp
    SettingsMacroParsingValues.cpp
    SettingsMacroProcessor.cpp
    SettingsMacroRoots.cpp
    ModuleResolver.cpp
    ProjectConfig.cpp
    PackageLock.cpp
    ProjectGenerator.cpp
    WebBuildPipeline.cpp
    WebShaderTranspiler.cpp
    WebDevServer.cpp
    ModuleCppManifest.cpp
    ModuleCppSupport.cpp
    MinimalBuilder.cpp
    PackageManager.cpp
    ProductBuilder.cpp
    ProductSettings.cpp
    ResourceCompiler.cpp
    installer/InstallerBuilder.cpp
)
target_include_directories(neuronc_cli PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(neuronc_cli PUBLIC ${LLVM_LIBS})
target_compile_definitions(neuronc_cli PUBLIC ${NPP_LLVM_DEFINES})

set(NCON_RUNTIME_ONLY_SOURCES
    Program.cpp
    Format.cpp
    Inspect.cpp
    Manifest.cpp
    NativeModuleManager.cpp
    Reader.cpp
    Runner.cpp
    RuntimeBridge.cpp
    Sandbox.cpp
    Sha256.cpp
    VM.cpp
    VMHotReload.cpp
    VMExecutorLifecycle.cpp
    VMExecutorValues.cpp
    VMExecutorOps.cpp
    VMExecutorTensor.cpp
    VMExecutorPatch.cpp
    NconMiniCLI.cpp
)

set(NCON_BUILD_SOURCES
    Builder.cpp
    Bytecode.cpp
    NconCLI.cpp
)

add_library(nconlib STATIC
    ${NCON_RUNTIME_ONLY_SOURCES}
    ${NCON_BUILD_SOURCES}
)
target_include_directories(nconlib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/runtime/include
    ${NPP_FFI_INCLUDE_DIR}
)
target_link_libraries(nconlib PUBLIC neuronc neuron_runtime ${NPP_FFI_LIBRARY})
if(WIN32)
    target_link_libraries(nconlib PUBLIC advapi32)
endif()

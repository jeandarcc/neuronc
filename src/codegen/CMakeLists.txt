add_library(neuronc_codegen OBJECT
    JITEngine.cpp
    LLVMCodeGen.cpp
    llvm/LLVMRuntimeRegistry.cpp
    llvm/LLVMTypeLowering.cpp
    llvm/LLVMObjectEmitter.cpp
)
target_include_directories(neuronc_codegen PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(neuronc_codegen PUBLIC ${LLVM_LIBS})
target_compile_definitions(neuronc_codegen PUBLIC ${Neuron_LLVM_DEFINES})

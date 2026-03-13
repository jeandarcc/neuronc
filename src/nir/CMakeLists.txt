add_library(neuronc_nir OBJECT
    NIR.cpp
    NIRBuilder.cpp
    decls/NIRBuilderDecls.cpp
    core/NIRBuilderCore.cpp
    core/NIRBuilderBlockVisitor.cpp
    lowering/LowerExpressions.cpp
    lowering/LowerExprOps.cpp
    lowering/LowerCalls.cpp
    lowering/LowerControlFlow.cpp
    lowering/LowerLoopAndExceptions.cpp
    lowering/LowerGraphics.cpp
    types/NIRTypeResolver.cpp
    Optimizer.cpp
    OptimizerUtils.cpp
    OptimizerCleanup.cpp
    OptimizerTensorFusion.cpp
    OptimizerGpuScopeLifting.cpp
)
target_include_directories(neuronc_nir PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(neuronc_nir PUBLIC ${LLVM_LIBS})
target_compile_definitions(neuronc_nir PUBLIC ${Neuron_LLVM_DEFINES})

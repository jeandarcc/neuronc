add_library(neuronc_mir OBJECT
    MIRBuilder.cpp
    MIRBuilderBindings.cpp
    MIRBuilderLowering.cpp
    MIROwnershipPass.cpp
    MIRPrinter.cpp
)
target_include_directories(neuronc_mir PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

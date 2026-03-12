add_library(neuronc_frontend OBJECT
    Diagnostics.cpp
    Frontend.cpp
)
target_include_directories(neuronc_frontend PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

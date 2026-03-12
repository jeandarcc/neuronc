add_library(neuronc_lexer OBJECT
    Lexer.cpp
    Token.cpp
)
target_include_directories(neuronc_lexer PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

add_library(neuronc_parser OBJECT
    AST.cpp
    Parser.cpp
    core/ParserCore.cpp
    declarations/ParseTopLevel.cpp
    declarations/ParseDeclarations.cpp
    declarations/ParseTypeDeclarations.cpp
    declarations/ParseGraphicsDeclarations.cpp
    expressions/ParseExpressions.cpp
    expressions/ParsePrimaryExpressions.cpp
    expressions/ParseExpressionSupport.cpp
    statements/ParseStatements.cpp
    statements/ParseControlFlow.cpp
    statements/ParseGraphicsStatements.cpp
)
target_include_directories(neuronc_parser PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

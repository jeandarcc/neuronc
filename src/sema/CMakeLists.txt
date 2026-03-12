add_library(neuronc_sema OBJECT
    AnalysisContext.cpp
    AnalysisHelpers.cpp
    BindingAnalyzer.cpp
    CallableBinder.cpp
    DeclarationAnalyzer.cpp
    DiagnosticEmitter.cpp
    ExpressionAnalyzer.cpp
    FlowAnalyzer.cpp
    GraphicsAnalyzer.cpp
    InputAnalyzer.cpp
    ReferenceTracker.cpp
    RuleValidator.cpp
    SemanticAnalyzer.cpp
    SemanticDriver.cpp
    ScopeManager.cpp
    StatementAnalyzer.cpp
    SymbolTable.cpp
    TypeChecker.cpp
    TypeResolver.cpp
    TypeSystem.cpp
)
target_include_directories(neuronc_sema PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

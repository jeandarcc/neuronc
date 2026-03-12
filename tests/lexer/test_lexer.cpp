// Lexer tests — included from test_main.cpp
#include "neuronc/lexer/Lexer.h"

using namespace neuron;

TEST(LexBasicBinding) {
    Lexer lexer("x is 10;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens.size(), 5u);
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[0].value, "x");
    ASSERT_EQ(tokens[1].type, TokenType::Is);
    ASSERT_EQ(tokens[2].type, TokenType::IntLiteral);
    ASSERT_EQ(tokens[2].value, "10");
    ASSERT_EQ(tokens[3].type, TokenType::Semicolon);
    ASSERT_EQ(tokens[4].type, TokenType::Eof);
    return true;
}

TEST(LexAnother) {
    Lexer lexer("y is another x;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[1].type, TokenType::Is);
    ASSERT_EQ(tokens[2].type, TokenType::Another);
    ASSERT_EQ(tokens[3].type, TokenType::Identifier);
    ASSERT_EQ(tokens[4].type, TokenType::Semicolon);
    return true;
}

TEST(LexAddressOf) {
    Lexer lexer("p is address of x;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[1].type, TokenType::Is);
    ASSERT_EQ(tokens[2].type, TokenType::AddressOf);
    ASSERT_EQ(tokens[3].type, TokenType::Identifier);
    ASSERT_EQ(tokens[4].type, TokenType::Semicolon);
    return true;
}

TEST(LexValueOf) {
    Lexer lexer("Print(value of p);");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[1].type, TokenType::LeftParen);
    ASSERT_EQ(tokens[2].type, TokenType::ValueOf);
    ASSERT_EQ(tokens[3].type, TokenType::Identifier);
    ASSERT_EQ(tokens[4].type, TokenType::RightParen);
    ASSERT_EQ(tokens[5].type, TokenType::Semicolon);
    return true;
}

TEST(LexTypeAnnotation) {
    Lexer lexer("x is 10 as int;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[2].type, TokenType::IntLiteral);
    ASSERT_EQ(tokens[3].type, TokenType::As);
    ASSERT_EQ(tokens[4].type, TokenType::Identifier);
    return true;
}

TEST(LexStringLiteral) {
    Lexer lexer("name is \"Neuron\" as string;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[2].type, TokenType::StringLiteral);
    ASSERT_EQ(tokens[2].value, "Neuron");
    return true;
}

TEST(LexFloatLiteral) {
    Lexer lexer("y is 3.14 as float;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[2].type, TokenType::FloatLiteral);
    ASSERT_EQ(tokens[2].value, "3.14");
    return true;
}

TEST(LexOperators) {
    Lexer lexer("x + y * z == 10 && a >= b");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[1].type, TokenType::Plus);
    ASSERT_EQ(tokens[3].type, TokenType::Star);
    ASSERT_EQ(tokens[5].type, TokenType::EqualEqual);
    ASSERT_EQ(tokens[7].type, TokenType::And);
    ASSERT_EQ(tokens[9].type, TokenType::GreaterEqual);
    return true;
}

TEST(LexIncrementAndAt) {
    Lexer lexer("x++; C is A @ B;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[1].type, TokenType::PlusPlus);
    ASSERT_EQ(tokens[5].type, TokenType::Identifier);
    ASSERT_EQ(tokens[6].type, TokenType::At);
    return true;
}

TEST(LexMethodDecl) {
    Lexer lexer("add is method(a as int, b as int) as int");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[1].type, TokenType::Is);
    ASSERT_EQ(tokens[2].type, TokenType::Method);
    ASSERT_EQ(tokens[3].type, TokenType::LeftParen);
    return true;
}

TEST(LexClassDecl) {
    Lexer lexer("Dog is public class inherits Animal");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[1].type, TokenType::Is);
    ASSERT_EQ(tokens[2].type, TokenType::Public);
    ASSERT_EQ(tokens[3].type, TokenType::Class);
    ASSERT_EQ(tokens[4].type, TokenType::Inherits);
    ASSERT_EQ(tokens[5].type, TokenType::Identifier);
    return true;
}

TEST(LexModuleDecl) {
    Lexer lexer("module Vector2;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Module);
    ASSERT_EQ(tokens[1].type, TokenType::Identifier);
    ASSERT_EQ(tokens[2].type, TokenType::Semicolon);
    return true;
}

TEST(LexModuleCppDecl) {
    Lexer lexer("modulecpp Tensorflow;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::ModuleCpp);
    ASSERT_EQ(tokens[1].type, TokenType::Identifier);
    ASSERT_EQ(tokens[1].value, "Tensorflow");
    ASSERT_EQ(tokens[2].type, TokenType::Semicolon);
    return true;
}
TEST(LexControlFlow) {
    Lexer lexer("if match default while for parallel break continue return");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::If);
    ASSERT_EQ(tokens[1].type, TokenType::Match);
    ASSERT_EQ(tokens[2].type, TokenType::Default);
    ASSERT_EQ(tokens[3].type, TokenType::While);
    ASSERT_EQ(tokens[4].type, TokenType::For);
    ASSERT_EQ(tokens[5].type, TokenType::Parallel);
    ASSERT_EQ(tokens[6].type, TokenType::Break);
    ASSERT_EQ(tokens[7].type, TokenType::Continue);
    ASSERT_EQ(tokens[8].type, TokenType::Return);
    return true;
}

TEST(LexLegacySwitchCaseAsIdentifiers) {
    Lexer lexer("switch case");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[0].value, "switch");
    ASSERT_EQ(tokens[1].type, TokenType::Identifier);
    ASSERT_EQ(tokens[1].value, "case");
    return true;
}

TEST(LexCastPipelineKeywords) {
    Lexer lexer("value as maybe string then float;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[1].type, TokenType::As);
    ASSERT_EQ(tokens[2].type, TokenType::Maybe);
    ASSERT_EQ(tokens[4].type, TokenType::Then);
    ASSERT_EQ(tokens[6].type, TokenType::Semicolon);
    return true;
}
TEST(LexErrorHandling) {
    Lexer lexer("try catch finally throw");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Try);
    ASSERT_EQ(tokens[1].type, TokenType::Catch);
    ASSERT_EQ(tokens[2].type, TokenType::Finally);
    ASSERT_EQ(tokens[3].type, TokenType::Throw);
    return true;
}

TEST(LexAsyncKeywords) {
    Lexer lexer("async await thread atomic");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Async);
    ASSERT_EQ(tokens[1].type, TokenType::Await);
    ASSERT_EQ(tokens[2].type, TokenType::Thread);
    ASSERT_EQ(tokens[3].type, TokenType::Atomic);
    return true;
}

TEST(LexGpuBlockTokens) {
    Lexer lexer("gpu { }");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens.size(), 4u);
    ASSERT_EQ(tokens[0].type, TokenType::Gpu);
    ASSERT_EQ(tokens[1].type, TokenType::LeftBrace);
    ASSERT_EQ(tokens[2].type, TokenType::RightBrace);
    ASSERT_EQ(tokens[3].type, TokenType::Eof);
  return true;
}

TEST(LexCanvasShaderPassTokens) {
    Lexer lexer("canvas shader pass");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Canvas);
    ASSERT_EQ(tokens[1].type, TokenType::Shader);
    ASSERT_EQ(tokens[2].type, TokenType::Pass);
    return true;
}

TEST(LexCSharpStyleTypeKeywords) {
    Lexer lexer("enum struct interface abstract virtual override overload");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Enum);
    ASSERT_EQ(tokens[1].type, TokenType::Struct);
    ASSERT_EQ(tokens[2].type, TokenType::Interface);
    ASSERT_EQ(tokens[3].type, TokenType::Abstract);
    ASSERT_EQ(tokens[4].type, TokenType::Virtual);
    ASSERT_EQ(tokens[5].type, TokenType::Override);
    ASSERT_EQ(tokens[6].type, TokenType::Overload);
    return true;
}
TEST(LexComments) {
    Lexer lexer("x is 10; // this is a comment\ny is 20;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens.size(), 9u);
    return true;
}

TEST(LexRangeOperator) {
    Lexer lexer("sub is matrix[0..10];");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[3].type, TokenType::LeftBracket);
    ASSERT_EQ(tokens[4].type, TokenType::IntLiteral);
    ASSERT_EQ(tokens[5].type, TokenType::DotDot);
    ASSERT_EQ(tokens[6].type, TokenType::IntLiteral);
    ASSERT_EQ(tokens[7].type, TokenType::RightBracket);
    return true;
}

TEST(LexFullProgram) {
    std::string source = R"(
        module Vector2;
        Init is method()
        {
            v is Vector2(3,4);
            Print(v.x);
        };
    )";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_TRUE(tokens.size() > 10);
    return true;
}

TEST(LexNullLiteral) {
    Lexer lexer("value is null;");
    auto tokens = lexer.tokenize();
    ASSERT_TRUE(lexer.errors().empty());
    ASSERT_EQ(tokens[0].type, TokenType::Identifier);
    ASSERT_EQ(tokens[1].type, TokenType::Is);
    ASSERT_EQ(tokens[2].type, TokenType::Null);
    ASSERT_EQ(tokens[3].type, TokenType::Semicolon);
    return true;
}

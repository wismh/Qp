#include "compiler/diagnostic.hpp"
#include "compiler/lexer.hpp"
#include "compiler/source.hpp"
#include "compiler/token.hpp"

#include <gtest/gtest.h>
#include <string_view>

using qpc::DiagnosticEngine;
using qpc::lex;
using qpc::Source;
using qpc::TokenKind;

TEST(Lexer, AddExpressionTokens) {
    auto src = Source::from_string("t.qp", "a + b");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].kind, TokenKind::Ident);
    EXPECT_EQ(tokens[0].text(src.view()), "a");
    EXPECT_EQ(tokens[1].kind, TokenKind::Plus);
    EXPECT_EQ(tokens[2].kind, TokenKind::Ident);
    EXPECT_EQ(tokens[2].text(src.view()), "b");
    EXPECT_EQ(tokens[3].kind, TokenKind::Eof);
}

TEST(Lexer, KeywordsArrowAndComments) {
    auto src = Source::from_string("t.qp", "pub fn add() -> i32 { /* c */ 1 // hi\n }");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    ASSERT_GE(tokens.size(), 10u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwPub);
    EXPECT_EQ(tokens[1].kind, TokenKind::KwFn);
    EXPECT_EQ(tokens[2].kind, TokenKind::Ident);
    EXPECT_EQ(tokens[3].kind, TokenKind::LParen);
    EXPECT_EQ(tokens[4].kind, TokenKind::RParen);
    EXPECT_EQ(tokens[5].kind, TokenKind::Arrow);
    EXPECT_EQ(tokens[6].kind, TokenKind::Ident);
    EXPECT_EQ(tokens[6].text(src.view()), "i32");
}

TEST(Lexer, IntegerAndFloatLiterals) {
    auto src = Source::from_string("t.qp", "10 10_000 3.14");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].kind, TokenKind::Int);
    EXPECT_EQ(tokens[0].text(src.view()), "10");
    EXPECT_EQ(tokens[1].kind, TokenKind::Int);
    EXPECT_EQ(tokens[1].text(src.view()), "10_000");
    EXPECT_EQ(tokens[2].kind, TokenKind::Float);
    EXPECT_EQ(tokens[2].text(src.view()), "3.14");
    EXPECT_EQ(tokens[3].kind, TokenKind::Eof);
}

TEST(Lexer, UnexpectedCharacter) {
    auto src = Source::from_string("t.qp", "@");
    DiagnosticEngine diags;
    lex(src, diags);
    ASSERT_TRUE(diags.has_errors());
    EXPECT_NE(diags.all().front().message.find("unexpected character"), std::string::npos);
}

TEST(Lexer, StructImplSelfAndDot) {
    auto src = Source::from_string("t.qp", "struct Vec2 impl self.x");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    ASSERT_EQ(tokens.size(), 7u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwStruct);
    EXPECT_EQ(tokens[1].kind, TokenKind::Ident);
    EXPECT_EQ(tokens[2].kind, TokenKind::KwImpl);
    EXPECT_EQ(tokens[3].kind, TokenKind::KwSelf);
    EXPECT_EQ(tokens[4].kind, TokenKind::Dot);
    EXPECT_EQ(tokens[5].kind, TokenKind::Ident);
    EXPECT_EQ(tokens[6].kind, TokenKind::Eof);
}

TEST(Lexer, StringsCharsBoolsAndMatchTokens) {
    auto src = Source::from_string("t.qp", "enum variant match for true false :: => \"hi\" 'x'");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors()) << diags.all().front().message;
    ASSERT_EQ(tokens.size(), 11u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwEnum);
    EXPECT_EQ(tokens[1].kind, TokenKind::KwVariant);
    EXPECT_EQ(tokens[2].kind, TokenKind::KwMatch);
    EXPECT_EQ(tokens[3].kind, TokenKind::KwFor);
    EXPECT_EQ(tokens[4].kind, TokenKind::KwTrue);
    EXPECT_EQ(tokens[5].kind, TokenKind::KwFalse);
    EXPECT_EQ(tokens[6].kind, TokenKind::ColonColon);
    EXPECT_EQ(tokens[7].kind, TokenKind::FatArrow);
    EXPECT_EQ(tokens[8].kind, TokenKind::String);
    EXPECT_EQ(tokens[8].text(src.view()), "\"hi\"");
        EXPECT_EQ(tokens[9].kind, TokenKind::Char);
        EXPECT_EQ(tokens[9].text(src.view()), "'x'");
        EXPECT_EQ(tokens[10].kind, TokenKind::Eof);
}

TEST(Lexer, Brackets) {
    auto src = Source::from_string("t.qp", "[1, 2]");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    EXPECT_EQ(tokens[0].kind, TokenKind::LBracket);
    EXPECT_EQ(tokens[4].kind, TokenKind::RBracket);
}

TEST(Lexer, ExternKeyword) {
    auto src = Source::from_string("t.qp", "extern \"C\" fn");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    EXPECT_EQ(tokens[0].kind, TokenKind::KwExtern);
    EXPECT_EQ(tokens[1].kind, TokenKind::String);
    EXPECT_EQ(tokens[1].text(src.view()), "\"C\"");
    EXPECT_EQ(tokens[2].kind, TokenKind::KwFn);
}

TEST(Lexer, ControlModAndCompareTokens) {
    auto src = Source::from_string("t.qp", "if else while in break continue mod use trait == != <= >= && || ! ..");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors()) << diags.all().front().message;
    ASSERT_EQ(tokens.size(), 18u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwIf);
    EXPECT_EQ(tokens[1].kind, TokenKind::KwElse);
    EXPECT_EQ(tokens[2].kind, TokenKind::KwWhile);
    EXPECT_EQ(tokens[3].kind, TokenKind::KwIn);
    EXPECT_EQ(tokens[4].kind, TokenKind::KwBreak);
    EXPECT_EQ(tokens[5].kind, TokenKind::KwContinue);
    EXPECT_EQ(tokens[6].kind, TokenKind::KwMod);
    EXPECT_EQ(tokens[7].kind, TokenKind::KwUse);
    EXPECT_EQ(tokens[8].kind, TokenKind::KwTrait);
    EXPECT_EQ(tokens[9].kind, TokenKind::EqEq);
    EXPECT_EQ(tokens[10].kind, TokenKind::BangEq);
    EXPECT_EQ(tokens[11].kind, TokenKind::Le);
    EXPECT_EQ(tokens[12].kind, TokenKind::Ge);
    EXPECT_EQ(tokens[13].kind, TokenKind::AmpAmp);
    EXPECT_EQ(tokens[14].kind, TokenKind::PipePipe);
    EXPECT_EQ(tokens[15].kind, TokenKind::Bang);
    EXPECT_EQ(tokens[16].kind, TokenKind::DotDot);
        EXPECT_EQ(tokens[17].kind, TokenKind::Eof);
}

TEST(Lexer, PipeAndOr) {
    auto src = Source::from_string("t.qp", "| ||");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].kind, TokenKind::Pipe);
    EXPECT_EQ(tokens[1].kind, TokenKind::PipePipe);
    EXPECT_EQ(tokens[2].kind, TokenKind::Eof);
}

TEST(Lexer, AsKeyword) {
    auto src = Source::from_string("t.qp", "as");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwAs);
    EXPECT_EQ(tokens[1].kind, TokenKind::Eof);
}

TEST(Lexer, NullAndQuestion) {
    auto src = Source::from_string("t.qp", "null i32?");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors()) << diags.all().front().message;
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwNull);
    EXPECT_EQ(tokens[1].kind, TokenKind::Ident);
    EXPECT_EQ(tokens[2].kind, TokenKind::Question);
    EXPECT_EQ(tokens[3].kind, TokenKind::Eof);
}

TEST(Lexer, NewKeyword) {
    auto src = Source::from_string("t.qp", "new");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwNew);
    EXPECT_EQ(tokens[1].kind, TokenKind::Eof);
}

TEST(Lexer, RefKeyword) {
    auto src = Source::from_string("t.qp", "ref");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwRef);
    EXPECT_EQ(tokens[1].kind, TokenKind::Eof);
}

TEST(Lexer, QuestionDotAndQuestionQuestion) {
    auto src = Source::from_string("t.qp", "?. ??");
    DiagnosticEngine diags;
    const auto tokens = lex(src, diags);
    ASSERT_FALSE(diags.has_errors());
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].kind, TokenKind::QuestionDot);
    EXPECT_EQ(tokens[1].kind, TokenKind::QuestionQuestion);
    EXPECT_EQ(tokens[2].kind, TokenKind::Eof);
}

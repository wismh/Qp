#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace qpc {

enum class TokenKind {
    Eof,
    Ident,
    Int,
    Float,
    KwFn,
    KwLet,
    KwMut,
    KwReturn,
    KwPub,
    KwStruct,
    KwImpl,
    KwSelf,
    LParen,
    RParen,
    LBrace,
    RBrace,
    Dot,
    Colon,
    Comma,
    Semicolon,
    Equal,
    Arrow,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::size_t offset = 0;
    std::size_t length = 0;

    [[nodiscard]] std::string_view text(std::string_view source) const {
        if (offset > source.size()) {
            return {};
        }
        return source.substr(offset, std::min(length, source.size() - offset));
    }
};

inline bool is_binop(TokenKind kind) {
    switch (kind) {
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:
        case TokenKind::Equal:
            return true;
        default:
            return false;
    }
}

inline const char* token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::Eof:
            return "eof";
        case TokenKind::Ident:
            return "identifier";
        case TokenKind::Int:
            return "integer";
        case TokenKind::Float:
            return "float";
        case TokenKind::KwFn:
            return "'fn'";
        case TokenKind::KwLet:
            return "'let'";
        case TokenKind::KwMut:
            return "'mut'";
        case TokenKind::KwReturn:
            return "'return'";
        case TokenKind::KwPub:
            return "'pub'";
        case TokenKind::KwStruct:
            return "'struct'";
        case TokenKind::KwImpl:
            return "'impl'";
        case TokenKind::KwSelf:
            return "'self'";
        case TokenKind::LParen:
            return "'('";
        case TokenKind::RParen:
            return "')'";
        case TokenKind::LBrace:
            return "'{'";
        case TokenKind::RBrace:
            return "'}'";
        case TokenKind::Dot:
            return "'.'";
        case TokenKind::Colon,
            return "':'";
        case TokenKind::Comma:
            return "','";
        case TokenKind::Semicolon:
            return "';'";
        case TokenKind::Equal:
            return "'='";
        case TokenKind::Arrow:
            return "'->'";
        case TokenKind::Plus:
            return "'+'";
        case TokenKind::Minus:
            return "'-'";
        case TokenKind::Star:
            return "'*'";
        case TokenKind::Slash:
            return "'/'";
        case TokenKind::Percent:
            return "'%'";
    }
    return "token";
}

}  // namespace qpc

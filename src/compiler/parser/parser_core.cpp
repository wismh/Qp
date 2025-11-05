#include "compiler/parser/parser_detail.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace qpc::detail {

AstFile Parser::parse_file() {
        AstFile file;

        while (!at(TokenKind::Eof)) {
            if (parse_item(file)) {
                continue;
            }
            const std::size_t before = pos_;
            recover_to_item();
            if (pos_ == before) {
                advance();
            }
        }

        return file;
    }

const Token& Parser::peek() const { return tokens_[pos_]; }

const Token& Parser::peek_n(std::size_t n) const {
        const std::size_t i = pos_ + n;
        return i < tokens_.size() ? tokens_[i] : tokens_.back();
    }

bool Parser::at(TokenKind kind) const { return peek().kind == kind; }

std::string_view Parser::peek_text() const { return peek().text(src_.view()); }

const Token& Parser::advance() {
        const Token& t = peek();
        if (t.kind != TokenKind::Eof) {
            ++pos_;
        }
        return t;
    }

bool Parser::consume(TokenKind kind) {
        if (!at(kind)) {
            return false;
        }
        advance();
        return true;
    }

bool Parser::expect(TokenKind kind, const char* what) {
        if (consume(kind)) {
            return true;
        }
        error(peek(), std::string("expected ") + what + ", found " + token_kind_name(peek().kind));
        return false;
    }

void Parser::error(const Token& tok, std::string message) {
        diags_.error(src_, tok.offset, std::move(message));
    }

void Parser::recover_to_item() {
        while (!at(TokenKind::Eof) && !at(TokenKind::KwFn) && !at(TokenKind::KwPub) &&
               !at(TokenKind::KwStruct) && !at(TokenKind::KwImpl) && !at(TokenKind::KwEnum) &&
               !at(TokenKind::KwVariant) && !at(TokenKind::KwExtern) && !at(TokenKind::KwLet) &&
               !at(TokenKind::KwMod) && !at(TokenKind::KwUse) && !at(TokenKind::KwFrom) &&
               !at(TokenKind::KwTrait)) {
            advance();
        }
    }

int Parser::binding_power(TokenKind kind) {
        switch (kind) {
            case TokenKind::Equal:
                return 1;
            case TokenKind::QuestionQuestion:
                return 2;
            case TokenKind::PipePipe:
                return 3;
            case TokenKind::AmpAmp:
                return 4;
            case TokenKind::EqEq:
            case TokenKind::BangEq:
                return 5;
            case TokenKind::Lt:
            case TokenKind::Le:
            case TokenKind::Gt:
            case TokenKind::Ge:
                return 6;
            case TokenKind::DotDot:
                return 7;
            case TokenKind::Plus:
            case TokenKind::Minus:
                return 8;
            case TokenKind::Star:
            case TokenKind::Slash:
            case TokenKind::Percent:
                return 9;
            default:
                return -1;
        }
    }

}  // namespace qpc::detail

#include "compiler/parser/parser_detail.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace qpc::detail {

std::optional<Block> Parser::parse_block() {
        Block block;
        block.offset = peek().offset;

        if (!expect(TokenKind::LBrace, "'{'")) {
            return std::nullopt;
        }

        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            if (at(TokenKind::KwLet) || at(TokenKind::KwReturn) || at(TokenKind::KwWhile) ||
                at(TokenKind::KwFor) || at(TokenKind::KwBreak) || at(TokenKind::KwContinue)) {
                auto stmt = parse_stmt_keyword();
                if (!stmt) {
                    return std::nullopt;
                }
                block.stmts.push_back(std::move(*stmt));
                continue;
            }

            auto expr = parse_expr();
            if (!expr) {
                return std::nullopt;
            }

            if (consume(TokenKind::Semicolon)) {
                const std::size_t off = (*expr)->offset;
                block.stmts.push_back(make_stmt(off, StmtExpr{std::move(*expr)}));
                continue;
            }

            if (at(TokenKind::RBrace)) {
                block.tail = std::move(*expr);
                break;
            }

            if (std::holds_alternative<ExprIf>((*expr)->kind)) {
                const std::size_t off = (*expr)->offset;
                block.stmts.push_back(make_stmt(off, StmtExpr{std::move(*expr)}));
                continue;
            }

            error(peek(), "expected ';' or '}' after expression");
            return std::nullopt;
        }

        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return block;
    }

std::optional<StmtPtr> Parser::parse_stmt_keyword() {
        if (at(TokenKind::KwLet)) {
            return parse_let();
        }
        if (at(TokenKind::KwWhile)) {
            return parse_while();
        }
        if (at(TokenKind::KwFor)) {
            return parse_for();
        }
        if (at(TokenKind::KwBreak)) {
            const std::size_t off = peek().offset;
            advance();
            if (!expect(TokenKind::Semicolon, "';'")) {
                return std::nullopt;
            }
            return make_stmt(off, StmtBreak{});
        }
        if (at(TokenKind::KwContinue)) {
            const std::size_t off = peek().offset;
            advance();
            if (!expect(TokenKind::Semicolon, "';'")) {
                return std::nullopt;
            }
            return make_stmt(off, StmtContinue{});
        }
        return parse_return();
    }

std::optional<StmtPtr> Parser::parse_while() {
        const std::size_t off = peek().offset;
        advance();
        auto cond = parse_expr(false);
        if (!cond) {
            return std::nullopt;
        }
        auto body = parse_block();
        if (!body) {
            return std::nullopt;
        }
        return make_stmt(off, StmtWhile{std::move(*cond), std::make_unique<Block>(std::move(*body))});
    }

std::optional<StmtPtr> Parser::parse_for() {
        const std::size_t off = peek().offset;
        advance();
        std::string name;
        std::string second;
        bool mut_name = false;
        bool mut_second = false;
        if (consume(TokenKind::LParen)) {
            mut_name = consume(TokenKind::KwMut);
            auto key = take_ident("loop variable");
            if (!key) {
                return std::nullopt;
            }
            name = std::move(*key);
            if (!expect(TokenKind::Comma, "','")) {
                return std::nullopt;
            }
            mut_second = consume(TokenKind::KwMut);
            auto val = take_ident("loop variable");
            if (!val) {
                return std::nullopt;
            }
            second = std::move(*val);
            if (!expect(TokenKind::RParen, "')' after loop variables")) {
                return std::nullopt;
            }
        } else {
            mut_name = consume(TokenKind::KwMut);
            auto ident = take_ident("loop variable");
            if (!ident) {
                return std::nullopt;
            }
            name = std::move(*ident);
        }
        if (!expect(TokenKind::KwIn, "'in'")) {
            return std::nullopt;
        }
        auto iter = parse_expr(false);
        if (!iter) {
            return std::nullopt;
        }
        auto body = parse_block();
        if (!body) {
            return std::nullopt;
        }
        StmtFor loop;
        loop.name = std::move(name);
        loop.second = std::move(second);
        loop.mut_name = mut_name;
        loop.mut_second = mut_second;
        loop.iter = std::move(*iter);
        loop.body = std::make_unique<Block>(std::move(*body));
        return make_stmt(off, std::move(loop));
    }

std::optional<StmtPtr> Parser::parse_let() {
        advance();

        StmtLet let;
        let.mut = consume(TokenKind::KwMut);

        const std::size_t name_off = peek().offset;
        auto name = take_ident("variable name");
        if (!name) {
            return std::nullopt;
        }
        let.name = std::move(*name);

        if (consume(TokenKind::Colon)) {
            auto ty = parse_type();
            if (!ty) {
                return std::nullopt;
            }
            let.ty = std::move(*ty);
        }

        if (!expect(TokenKind::Equal, "'='")) {
            return std::nullopt;
        }

        auto init = parse_expr();
        if (!init) {
            return std::nullopt;
        }
        let.init = std::move(*init);

        if (!expect(TokenKind::Semicolon, "';'")) {
            return std::nullopt;
        }
        return make_stmt(name_off, std::move(let));
    }

std::optional<StmtPtr> Parser::parse_return() {
        const std::size_t off = peek().offset;
        advance();

        StmtReturn ret;
        if (!at(TokenKind::Semicolon) && !at(TokenKind::RBrace)) {
            auto value = parse_expr();
            if (!value) {
                return std::nullopt;
            }
            ret.value = std::move(*value);
        }

        if (!expect(TokenKind::Semicolon, "';'")) {
            return std::nullopt;
        }
        return make_stmt(off, std::move(ret));
    }

}  // namespace qpc::detail

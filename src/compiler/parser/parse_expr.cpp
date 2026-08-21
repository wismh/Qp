#include "compiler/parser/parser_detail.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace qpc::detail {

std::optional<ExprPtr> Parser::parse_expr(bool allow_struct) { return parse_prec(1, allow_struct); }

std::optional<ExprPtr> Parser::parse_prec(int min_bp, bool allow_struct) {
        auto left = parse_unary(allow_struct);
        if (!left) {
            return std::nullopt;
        }

        while (true) {
            const TokenKind op = peek().kind;
            const int bp = binding_power(op);
            if (bp < min_bp) {
                break;
            }

            const std::size_t op_off = peek().offset;
            advance();

            if (op == TokenKind::Equal) {
                auto rhs = parse_prec(bp, allow_struct);
                if (!rhs) {
                    return std::nullopt;
                }
                left = make_expr(op_off, ExprAssign{std::move(*left), std::move(*rhs)});
                continue;
            }
            if (op == TokenKind::DotDot) {
                auto rhs = parse_prec(bp + 1, allow_struct);
                if (!rhs) {
                    return std::nullopt;
                }
                left = make_expr(op_off, ExprRange{std::move(*left), std::move(*rhs)});
                continue;
            }
            if (op == TokenKind::QuestionQuestion) {
                auto rhs = parse_prec(bp + 1, allow_struct);
                if (!rhs) {
                    return std::nullopt;
                }
                left = make_expr(op_off, ExprCoalesce{std::move(*left), std::move(*rhs)});
                continue;
            }

            auto rhs = parse_prec(bp + 1, allow_struct);
            if (!rhs) {
                return std::nullopt;
            }
            left = make_expr(op_off, ExprBinary{op, std::move(*left), std::move(*rhs)});
        }

        return left;
    }

std::optional<ExprPtr> Parser::parse_unary(bool allow_struct) {
        if (at(TokenKind::Minus) || at(TokenKind::Bang)) {
            const std::size_t off = peek().offset;
            const TokenKind op = peek().kind;
            advance();
            auto operand = parse_unary(allow_struct);
            if (!operand) {
                return std::nullopt;
            }
            return make_expr(off, ExprUnary{op, std::move(*operand)});
        }
        return parse_postfix(allow_struct);
    }

std::optional<ExprPtr> Parser::parse_postfix(bool allow_struct) {
        auto expr = parse_primary(allow_struct);
        if (!expr) {
            return std::nullopt;
        }

        while (true) {
            if (at(TokenKind::LParen)) {
                expr = parse_call(std::move(*expr), {});
                if (!expr) {
                    return std::nullopt;
                }
                continue;
            }
            if (at(TokenKind::Lt)) {
                auto targs = try_type_args();
                if (targs && at(TokenKind::LParen)) {
                    expr = parse_call(std::move(*expr), std::move(*targs));
                    if (!expr) {
                        return std::nullopt;
                    }
                    continue;
                }
            }
            if (at(TokenKind::Dot)) {
                expr = parse_field(std::move(*expr), false);
                if (!expr) {
                    return std::nullopt;
                }
                continue;
            }
            if (at(TokenKind::QuestionDot)) {
                expr = parse_field(std::move(*expr), true);
                if (!expr) {
                    return std::nullopt;
                }
                continue;
            }
            if (at(TokenKind::LBracket)) {
                expr = parse_index(std::move(*expr));
                if (!expr) {
                    return std::nullopt;
                }
                continue;
            }
            if (at(TokenKind::KwAs)) {
                const std::size_t off = peek().offset;
                advance();
                auto ty = parse_type();
                if (!ty) {
                    return std::nullopt;
                }
                expr = make_expr(off, ExprCast{std::move(*expr), std::move(*ty)});
                continue;
            }
            if (at(TokenKind::Bang)) {
                const std::size_t off = peek().offset;
                advance();
                expr = make_expr(off, ExprUnwrap{std::move(*expr)});
                continue;
            }
            if (at(TokenKind::Question)) {
                const std::size_t off = peek().offset;
                advance();
                expr = make_expr(off, ExprTry{std::move(*expr)});
                continue;
            }
            break;
        }
        return expr;
    }

std::optional<ExprPtr> Parser::parse_index(ExprPtr base) {
        const std::size_t off = peek().offset;
        advance();
        auto index = parse_expr();
        if (!index) {
            return std::nullopt;
        }
        if (!expect(TokenKind::RBracket, "']'")) {
            return std::nullopt;
        }
        return make_expr(off, ExprIndex{std::move(base), std::move(*index)});
    }

std::optional<ExprPtr> Parser::parse_call(ExprPtr callee, std::vector<TypeExpr> type_args) {
        const std::size_t off = peek().offset;
        advance();

        auto args = parse_arg_list();
        if (!args) {
            return std::nullopt;
        }
        return make_expr(off, ExprCall{std::move(callee), std::move(type_args), std::move(*args)});
    }

std::optional<ExprPtr> Parser::parse_struct_lit(std::size_t off, std::vector<std::string> path,
 std::vector<TypeExpr> type_args) {
        advance();

        std::vector<StructLitField> fields;
        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            auto field_name = take_ident("field name");
            if (!field_name) {
                return std::nullopt;
            }
            if (!expect(TokenKind::Colon, "':'")) {
                return std::nullopt;
            }
            auto value = parse_expr();
            if (!value) {
                return std::nullopt;
            }
            fields.push_back(StructLitField{std::move(*field_name), std::move(*value)});
            if (!consume(TokenKind::Comma)) {
                break;
            }
        }

        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        ExprStructLit lit;
        if (!path.empty()) {
            lit.name = path[0];
            for (std::size_t i = 1; i < path.size(); ++i) {
                lit.name += "::";
                lit.name += path[i];
            }
        }
        lit.path = std::move(path);
        lit.type_args = std::move(type_args);
        lit.fields = std::move(fields);
        return make_expr(off, std::move(lit));
    }

std::optional<ExprPtr> Parser::parse_new() {
        const std::size_t off = peek().offset;
        advance();
        std::vector<std::string> path;
        auto first = take_ident("type name after 'new'");
        if (!first) {
            return std::nullopt;
        }
        path.push_back(std::move(*first));
        while (consume(TokenKind::ColonColon)) {
            auto part = take_ident("type path");
            if (!part) {
                return std::nullopt;
            }
            path.push_back(std::move(*part));
        }
        auto targs = try_type_args();
        if (!at(TokenKind::LBrace)) {
            error(peek(), "expected '{' after 'new Type'");
            return std::nullopt;
        }
        auto lit = parse_struct_lit(off, std::move(path), targs ? std::move(*targs) : std::vector<TypeExpr>{});
        if (!lit) {
            return std::nullopt;
        }
        auto* st = std::get_if<ExprStructLit>(&(*lit)->kind);
        if (!st) {
            error(peek(), "expected struct literal after 'new'");
            return std::nullopt;
        }
        ExprNew n;
        n.path = std::move(st->path);
        n.name = n.path.empty() ? st->name : n.path.back();
        n.type_args = std::move(st->type_args);
        n.fields = std::move(st->fields);
        (*lit)->kind = std::move(n);
        return lit;
    }

bool Parser::is_type_suffix(std::string_view text) {
        return text == "i8" || text == "i16" || text == "i32" || text == "i64" || text == "u8" ||
               text == "u16" || text == "u32" || text == "u64" || text == "f32" || text == "f64" ||
               text == "byte";
    }

std::optional<std::string> Parser::take_suffix() {
        if (!at(TokenKind::Ident) || !is_type_suffix(peek_text())) {
            return std::nullopt;
        }
        std::string suffix(peek_text());
        advance();
        return suffix;
    }

std::optional<PatPtr> Parser::parse_pat() {
        auto pat = std::make_unique<Pat>();
        pat->offset = peek().offset;

        if (at(TokenKind::Ident) && peek_text() == "_") {
            advance();
            pat->kind = PatWild{};
            return pat;
        }

        if (!at(TokenKind::Ident)) {
            error(peek(), "expected pattern");
            return std::nullopt;
        }

        std::vector<std::string> path;
        path.emplace_back(peek_text());
        advance();
        while (consume(TokenKind::ColonColon)) {
            auto part = take_ident("path segment");
            if (!part) {
                return std::nullopt;
            }
            path.push_back(std::move(*part));
        }

        if (consume(TokenKind::LBrace)) {
            PatVariant v;
            v.path = std::move(path);
            while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
                auto field = take_ident("field name");
                if (!field) {
                    return std::nullopt;
                }
                v.fields.push_back(std::move(*field));
                consume(TokenKind::Comma);
            }
            if (!expect(TokenKind::RBrace, "'}'")) {
                return std::nullopt;
            }
            pat->kind = std::move(v);
            return pat;
        }

        if (consume(TokenKind::LParen)) {
            PatVariant v;
            v.path = std::move(path);
            v.tuple = true;
            while (!at(TokenKind::Eof) && !at(TokenKind::RParen)) {
                auto inner = parse_pat();
                if (!inner) {
                    return std::nullopt;
                }
                v.args.push_back(std::move(*inner));
                if (!consume(TokenKind::Comma)) {
                    break;
                }
            }
            if (!expect(TokenKind::RParen, "')'")) {
                return std::nullopt;
            }
            pat->kind = std::move(v);
            return pat;
        }

        if (path.size() == 1) {
            pat->kind = PatIdent{std::move(path[0])};
        } else {
            PatVariant v;
            v.path = std::move(path);
            pat->kind = std::move(v);
        }
        return pat;
    }

std::optional<ExprPtr> Parser::parse_closure(bool by_ref) {
        const std::size_t off = peek().offset;
        ExprClosure clo;
        clo.by_ref = by_ref;
        if (consume(TokenKind::PipePipe)) {
        } else {
            if (!expect(TokenKind::Pipe, "'|'")) {
                return std::nullopt;
            }
            if (!at(TokenKind::Pipe)) {
                while (true) {
                    ClosureParam p;
                    p.offset = peek().offset;
                    auto name = take_ident("closure parameter");
                    if (!name) {
                        return std::nullopt;
                    }
                    p.name = std::move(*name);
                    if (!expect(TokenKind::Colon, "':' after closure parameter")) {
                        return std::nullopt;
                    }
                    auto ty = parse_type();
                    if (!ty) {
                        return std::nullopt;
                    }
                    p.ty = std::move(*ty);
                    clo.params.push_back(std::move(p));
                    if (!consume(TokenKind::Comma)) {
                        break;
                    }
                    if (at(TokenKind::Pipe)) {
                        break;
                    }
                }
            }
            if (!expect(TokenKind::Pipe, "'|' after closure parameters")) {
                return std::nullopt;
            }
        }
        if (consume(TokenKind::Arrow)) {
            auto ret = parse_type();
            if (!ret) {
                return std::nullopt;
            }
            clo.return_ty = std::move(*ret);
            auto body = parse_block();
            if (!body) {
                return std::nullopt;
            }
            clo.body = std::make_unique<Block>(std::move(*body));
        } else if (at(TokenKind::LBrace)) {
            auto body = parse_block();
            if (!body) {
                return std::nullopt;
            }
            clo.body = std::make_unique<Block>(std::move(*body));
        } else {
            auto tail = parse_expr();
            if (!tail) {
                return std::nullopt;
            }
            Block body;
            body.offset = (*tail)->offset;
            body.tail = std::move(*tail);
            clo.body = std::make_unique<Block>(std::move(body));
        }
        return make_expr(off, std::move(clo));
    }

std::optional<ExprPtr> Parser::parse_if() {
        const std::size_t off = peek().offset;
        if (!expect(TokenKind::KwIf, "'if'")) {
            return std::nullopt;
        }
        ExprIf node;
        if (consume(TokenKind::KwLet)) {
            auto name = take_ident("binding after 'if let'");
            if (!name) {
                return std::nullopt;
            }
            node.let_name = std::move(*name);
            if (!expect(TokenKind::Equal, "'=' after if-let binding")) {
                return std::nullopt;
            }
        }
        auto cond = parse_expr(false);
        if (!cond) {
            return std::nullopt;
        }
        auto then_block = parse_block();
        if (!then_block) {
            return std::nullopt;
        }
        node.cond = std::move(*cond);
        node.then_block = std::make_unique<Block>(std::move(*then_block));
        if (consume(TokenKind::KwElse)) {
            if (at(TokenKind::KwIf)) {
                auto els = parse_if();
                if (!els) {
                    return std::nullopt;
                }
                node.else_expr = std::move(*els);
            } else {
                auto else_block = parse_block();
                if (!else_block) {
                    return std::nullopt;
                }
                ExprIf always;
                always.cond = make_expr(off, LitBool{true});
                always.then_block = std::make_unique<Block>(std::move(*else_block));
                node.else_expr = make_expr(off, std::move(always));
            }
        }
        return make_expr(off, std::move(node));
    }

std::optional<ExprPtr> Parser::parse_match() {
        const std::size_t off = peek().offset;
        advance();

        auto scrutinee = parse_expr(false);
        if (!scrutinee) {
            return std::nullopt;
        }
        if (!expect(TokenKind::LBrace, "'{'")) {
            return std::nullopt;
        }

        ExprMatch match;
        match.scrutinee = std::move(*scrutinee);
        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            auto pat = parse_pat();
            if (!pat) {
                return std::nullopt;
            }
            if (!expect(TokenKind::FatArrow, "'=>'")) {
                return std::nullopt;
            }
            auto body = parse_expr();
            if (!body) {
                return std::nullopt;
            }
            match.arms.push_back(MatchArm{std::move(*pat), std::move(*body)});
            consume(TokenKind::Comma);
        }

        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return make_expr(off, std::move(match));
    }

std::optional<std::vector<ExprPtr>> Parser::parse_arg_list() {
        std::vector<ExprPtr> args;
        if (at(TokenKind::RParen)) {
            advance();
            return args;
        }

        while (true) {
            auto arg = parse_expr();
            if (!arg) {
                return std::nullopt;
            }
            args.push_back(std::move(*arg));

            if (consume(TokenKind::Comma)) {
                if (at(TokenKind::RParen)) {
                    break;
                }
                continue;
            }
            break;
        }

        if (!expect(TokenKind::RParen, "')'")) {
            return std::nullopt;
        }
        return args;
    }

std::optional<ExprPtr> Parser::parse_string_interp() {
        const std::size_t off = peek().offset;
        ExprPtr acc;
        auto append = [&](ExprPtr part) {
            if (!acc) {
                acc = std::move(part);
            } else {
                acc = make_expr(off, ExprBinary{TokenKind::Plus, std::move(acc), std::move(part)});
            }
        };

        while (at(TokenKind::StringFrag)) {
            const std::size_t frag_off = peek().offset;
            std::string raw = "\"";
            raw.append(peek_text());
            raw.push_back('"');
            advance();
            append(make_expr(frag_off, LitString{std::move(raw)}));

            if (!consume(TokenKind::DollarBrace)) {
                break;
            }
            auto inner = parse_expr(true);
            if (!inner) {
                return std::nullopt;
            }
            if (!expect(TokenKind::RBrace, "'}' after interpolation")) {
                return std::nullopt;
            }
            auto callee = make_expr((*inner)->offset, ExprIdent{"to_string"});
            std::vector<ExprPtr> args;
            args.push_back(std::move(*inner));
            append(make_expr(frag_off, ExprCall{std::move(callee), {}, std::move(args)}));
        }

        if (!acc) {
            diags_.error(src_, off, "expected string literal");
            return std::nullopt;
        }
        return acc;
    }

std::optional<ExprPtr> Parser::parse_primary(bool allow_struct) {
        if (at(TokenKind::KwIf)) {
            return parse_if();
        }

        if (at(TokenKind::KwRef) &&
            (peek_n(1).kind == TokenKind::Pipe || peek_n(1).kind == TokenKind::PipePipe)) {
            advance();
            return parse_closure(true);
        }

        if (at(TokenKind::Pipe) || at(TokenKind::PipePipe)) {
            return parse_closure(false);
        }

        if (at(TokenKind::KwNew)) {
            return parse_new();
        }

        if (at(TokenKind::KwMatch)) {
            return parse_match();
        }

        if (at(TokenKind::KwTrue) || at(TokenKind::KwFalse)) {
            const std::size_t off = peek().offset;
            const bool value = at(TokenKind::KwTrue);
            advance();
            return make_expr(off, LitBool{value});
        }

        if (at(TokenKind::KwNull)) {
            const std::size_t off = peek().offset;
            advance();
            return make_expr(off, LitNull{});
        }

        if (at(TokenKind::String)) {
            const std::size_t off = peek().offset;
            std::string raw(peek_text());
            advance();
            return make_expr(off, LitString{std::move(raw)});
        }

        if (at(TokenKind::StringFrag)) {
            return parse_string_interp();
        }

        if (at(TokenKind::Char)) {
            const std::size_t off = peek().offset;
            std::string raw(peek_text());
            advance();
            return make_expr(off, LitChar{std::move(raw)});
        }

        if (at(TokenKind::Int) || at(TokenKind::Float)) {
            const std::size_t off = peek().offset;
            const std::string raw(peek_text());
            const bool is_float = at(TokenKind::Float);
            advance();
            auto suffix = take_suffix();
            if (is_float) {
                return make_expr(off, LitFloat{raw, std::move(suffix)});
            }
            return make_expr(off, LitInt{raw, std::move(suffix)});
        }

        if (at(TokenKind::KwSelf)) {
            const std::size_t off = peek().offset;
            advance();
            return make_expr(off, ExprIdent{"self"});
        }

        if (at(TokenKind::Ident)) {
            const std::size_t off = peek().offset;
            std::vector<std::string> path;
            path.emplace_back(peek_text());
            advance();
            while (consume(TokenKind::ColonColon)) {
                auto part = take_ident("path segment");
                if (!part) {
                    return std::nullopt;
                }
                path.push_back(std::move(*part));
            }
            const std::size_t saved = pos_;
            const std::size_t diags_at = diags_.size();
            auto targs = try_type_args();
            if (allow_struct && at(TokenKind::LBrace)) {
                return parse_struct_lit(off, std::move(path),
                                        targs ? std::move(*targs) : std::vector<TypeExpr>{});
            }
            if (targs) {
                pos_ = saved;
                diags_.truncate(diags_at);
            }
            if (path.size() == 1) {
                return make_expr(off, ExprIdent{std::move(path[0])});
            }
            return make_expr(off, ExprPath{std::move(path)});
        }

        if (at(TokenKind::LParen)) {
            const std::size_t off = peek().offset;
            advance();
            auto first = parse_expr();
            if (!first) {
                return std::nullopt;
            }
            if (consume(TokenKind::RParen)) {
                return first;
            }
            if (!expect(TokenKind::Comma, "',' in tuple")) {
                return std::nullopt;
            }
            ExprTuple lit;
            lit.elems.push_back(std::move(*first));
            while (!at(TokenKind::RParen) && !at(TokenKind::Eof)) {
                auto next = parse_expr();
                if (!next) {
                    return std::nullopt;
                }
                lit.elems.push_back(std::move(*next));
                if (!consume(TokenKind::Comma)) {
                    break;
                }
            }
            if (!expect(TokenKind::RParen, "')' after tuple")) {
                return std::nullopt;
            }
            if (lit.elems.size() < 2) {
                error(peek(), "tuple literal needs at least two elements");
                return std::nullopt;
            }
            return make_expr(off, std::move(lit));
        }

        if (at(TokenKind::LBracket)) {
            return parse_list_lit();
        }

        if (at(TokenKind::LBrace)) {
            return parse_dict_lit();
        }

        error(peek(), std::string("expected expression, found ") + token_kind_name(peek().kind));
        return std::nullopt;
    }

std::optional<ExprPtr> Parser::parse_list_lit() {
        const std::size_t off = peek().offset;
        advance();
        ExprListLit lit;
        while (!at(TokenKind::Eof) && !at(TokenKind::RBracket)) {
            auto elem = parse_expr();
            if (!elem) {
                return std::nullopt;
            }
            lit.elems.push_back(std::move(*elem));
            if (!consume(TokenKind::Comma)) {
                break;
            }
        }
        if (!expect(TokenKind::RBracket, "']'")) {
            return std::nullopt;
        }
        return make_expr(off, std::move(lit));
    }

std::optional<ExprPtr> Parser::parse_dict_lit() {
        const std::size_t off = peek().offset;
        advance();
        ExprDictLit lit;
        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            auto key = parse_expr();
            if (!key) {
                return std::nullopt;
            }
            if (!expect(TokenKind::Colon, "':'")) {
                return std::nullopt;
            }
            auto value = parse_expr();
            if (!value) {
                return std::nullopt;
            }
            lit.entries.push_back(ExprDictEntry{std::move(*key), std::move(*value)});
            if (!consume(TokenKind::Comma)) {
                break;
            }
        }
        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return make_expr(off, std::move(lit));
    }

}  // namespace qpc::detail

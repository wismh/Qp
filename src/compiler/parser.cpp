#include "compiler/parser.hpp"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace qpc {
namespace {

template <class Kind>
ExprPtr make_expr(std::size_t offset, Kind kind) {
    auto expr = std::make_unique<Expr>();
    expr->offset = offset;
    expr->kind = std::move(kind);
    return expr;
}

template <class Kind>
StmtPtr make_stmt(std::size_t offset, Kind kind) {
    auto stmt = std::make_unique<Stmt>();
    stmt->offset = offset;
    stmt->kind = std::move(kind);
    return stmt;
}

class Parser {
public:
    Parser(const Source& src, const std::vector<Token>& tokens, DiagnosticEngine& diags)
        : src_(src), tokens_(tokens), diags_(diags) {}

    AstFile parse_file() {
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

private:
    const Source& src_;
    const std::vector<Token>& tokens_;
    DiagnosticEngine& diags_;
    std::size_t pos_ = 0;

    const Token& peek() const { return tokens_[pos_]; }

    const Token& peek_n(std::size_t n) const {
        const std::size_t i = pos_ + n;
        return i < tokens_.size() ? tokens_[i] : tokens_.back();
    }

    bool at(TokenKind kind) const { return peek().kind == kind; }

    std::string_view peek_text() const { return peek().text(src_.view()); }

    const Token& advance() {
        const Token& t = peek();
        if (t.kind != TokenKind::Eof) {
            ++pos_;
        }
        return t;
    }

    bool consume(TokenKind kind) {
        if (!at(kind)) {
            return false;
        }
        advance();
        return true;
    }

    bool expect(TokenKind kind, const char* what) {
        if (consume(kind)) {
            return true;
        }
        error(peek(), std::string("expected ") + what + ", found " + token_kind_name(peek().kind));
        return false;
    }

    void error(const Token& tok, std::string message) {
        diags_.error(src_, tok.offset, std::move(message));
    }

    void recover_to_item() {
        while (!at(TokenKind::Eof) && !at(TokenKind::KwFn) && !at(TokenKind::KwPub) &&
               !at(TokenKind::KwStruct) && !at(TokenKind::KwImpl) && !at(TokenKind::KwEnum) &&
               !at(TokenKind::KwVariant)) {
            advance();
        }
    }

    bool parse_item(AstFile& file) {
        if (at(TokenKind::KwImpl)) {
            auto impl = parse_impl();
            if (!impl) {
                return false;
            }
            file.impls.push_back(std::move(*impl));
            return true;
        }

        if (at(TokenKind::KwEnum) || at(TokenKind::KwVariant) ||
            (at(TokenKind::KwPub) &&
             (peek_n(1).kind == TokenKind::KwEnum || peek_n(1).kind == TokenKind::KwVariant))) {
            auto en = parse_enum();
            if (!en) {
                return false;
            }
            file.enums.push_back(std::move(*en));
            return true;
        }

        if (at(TokenKind::KwStruct) ||
            (at(TokenKind::KwPub) && peek_n(1).kind == TokenKind::KwStruct)) {
            auto st = parse_struct();
            if (!st) {
                return false;
            }
            file.structs.push_back(std::move(*st));
            return true;
        }

        if (at(TokenKind::KwFn) || (at(TokenKind::KwPub) && peek_n(1).kind == TokenKind::KwFn)) {
            auto fn = parse_fn(false);
            if (!fn) {
                return false;
            }
            file.functions.push_back(std::move(*fn));
            return true;
        }

        error(peek(), std::string("expected item, found ") + token_kind_name(peek().kind));
        return false;
    }

    std::optional<std::string> take_ident(const char* what) {
        if (!at(TokenKind::Ident)) {
            error(peek(), std::string("expected ") + what);
            return std::nullopt;
        }

        std::string name(peek_text());
        advance();
        return name;
    }

    std::optional<StructDecl> parse_struct() {
        StructDecl st;
        st.offset = peek().offset;
        st.pub = consume(TokenKind::KwPub);

        if (!expect(TokenKind::KwStruct, "'struct'")) {
            return std::nullopt;
        }

        const std::size_t name_off = peek().offset;
        auto name = take_ident("struct name");
        if (!name) {
            return std::nullopt;
        }
        st.name = std::move(*name);
        st.offset = name_off;

        if (!expect(TokenKind::LBrace, "'{'")) {
            return std::nullopt;
        }

        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            auto field = parse_field();
            if (!field) {
                return std::nullopt;
            }
            st.fields.push_back(std::move(*field));
            consume(TokenKind::Comma);
        }

        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return st;
    }

    std::optional<FieldDecl> parse_field() {
        FieldDecl field;
        field.offset = peek().offset;
        field.pub = consume(TokenKind::KwPub);
        field.mut = consume(TokenKind::KwMut);

        auto name = take_ident("field name");
        if (!name) {
            return std::nullopt;
        }
        field.name = std::move(*name);

        if (!expect(TokenKind::Colon, "':'")) {
            return std::nullopt;
        }

        auto ty = parse_type_name();
        if (!ty) {
            return std::nullopt;
        }
        field.ty = std::move(*ty);
        return field;
    }

    std::optional<EnumDecl> parse_enum() {
        EnumDecl en;
        en.offset = peek().offset;
        en.pub = consume(TokenKind::KwPub);

        if (!at(TokenKind::KwEnum) && !at(TokenKind::KwVariant)) {
            error(peek(), "expected 'enum' or 'variant'");
            return std::nullopt;
        }
        advance();

        const std::size_t name_off = peek().offset;
        auto name = take_ident("enum name");
        if (!name) {
            return std::nullopt;
        }
        en.name = std::move(*name);
        en.offset = name_off;

        if (!expect(TokenKind::LBrace, "'{'")) {
            return std::nullopt;
        }

        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            auto variant = parse_variant();
            if (!variant) {
                return std::nullopt;
            }
            en.variants.push_back(std::move(*variant));
            consume(TokenKind::Comma);
        }

        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return en;
    }

    std::optional<VariantDecl> parse_variant() {
        VariantDecl v;
        v.offset = peek().offset;
        auto name = take_ident("variant name");
        if (!name) {
            return std::nullopt;
        }
        v.name = std::move(*name);

        if (consume(TokenKind::LBrace)) {
            while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
                auto field = parse_field();
                if (!field) {
                    return std::nullopt;
                }
                v.fields.push_back(std::move(*field));
                consume(TokenKind::Comma);
            }
            if (!expect(TokenKind::RBrace, "'}'")) {
                return std::nullopt;
            }
            return v;
        }

        if (consume(TokenKind::LParen)) {
            v.tuple = true;
            std::size_t index = 0;
            while (!at(TokenKind::Eof) && !at(TokenKind::RParen)) {
                FieldDecl field;
                field.offset = peek().offset;
                field.name = "_" + std::to_string(index++);
                auto ty = parse_type_name();
                if (!ty) {
                    return std::nullopt;
                }
                field.ty = std::move(*ty);
                v.fields.push_back(std::move(field));
                if (!consume(TokenKind::Comma)) {
                    break;
                }
            }
            if (!expect(TokenKind::RParen, "')'")) {
                return std::nullopt;
            }
        }
        return v;
    }

    std::optional<ImplDecl> parse_impl() {
        ImplDecl impl;
        impl.offset = peek().offset;

        if (!expect(TokenKind::KwImpl, "'impl'")) {
            return std::nullopt;
        }

        auto name = take_ident("type or trait name");
        if (!name) {
            return std::nullopt;
        }

        if (consume(TokenKind::KwFor)) {
            impl.trait_name = std::move(*name);
            auto type_name = take_ident("type name");
            if (!type_name) {
                return std::nullopt;
            }
            impl.type_name = std::move(*type_name);
        } else {
            impl.type_name = std::move(*name);
        }

        if (!expect(TokenKind::LBrace, "'{'")) {
            return std::nullopt;
        }

        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            auto method = parse_fn(true);
            if (!method) {
                return std::nullopt;
            }
            impl.methods.push_back(std::move(*method));
        }

        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return impl;
    }

    std::optional<FnDecl> parse_fn(bool in_impl) {
        FnDecl fn;
        fn.offset = peek().offset;
        fn.pub = consume(TokenKind::KwPub);

        if (!expect(TokenKind::KwFn, "'fn'")) {
            return std::nullopt;
        }

        fn.offset = peek().offset;
        auto name = take_ident("function name");
        if (!name) {
            return std::nullopt;
        }
        fn.name = std::move(*name);

        auto params = parse_params(fn, in_impl);
        if (!params) {
            return std::nullopt;
        }
        fn.params = std::move(*params);

        if (consume(TokenKind::Arrow)) {
            auto ty = parse_type_name();
            if (!ty) {
                return std::nullopt;
            }
            fn.return_ty = std::move(*ty);
        }

        auto body = parse_block();
        if (!body) {
            return std::nullopt;
        }
        fn.body = std::move(*body);
        return fn;
    }

    bool consume_self_param(FnDecl& fn) {
        if (at(TokenKind::KwMut) && peek_n(1).kind == TokenKind::KwSelf) {
            fn.self_param = SelfParam::Mut;
            advance();
            advance();
            return true;
        }
        if (at(TokenKind::KwSelf)) {
            fn.self_param = SelfParam::Value;
            advance();
            return true;
        }
        return false;
    }

    std::optional<std::vector<Param>> parse_params(FnDecl& fn, bool in_impl) {
        if (!expect(TokenKind::LParen, "'('")) {
            return std::nullopt;
        }

        std::vector<Param> params;
        if (at(TokenKind::RParen)) {
            advance();
            return params;
        }

        if (in_impl && consume_self_param(fn)) {
            if (at(TokenKind::RParen)) {
                advance();
                return params;
            }
            if (!expect(TokenKind::Comma, "',' after self")) {
                return std::nullopt;
            }
            if (at(TokenKind::RParen)) {
                advance();
                return params;
            }
        }

        while (true) {
            auto p = parse_param();
            if (!p) {
                return std::nullopt;
            }
            params.push_back(std::move(*p));

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
        return params;
    }

    std::optional<Param> parse_param() {
        Param p;
        p.offset = peek().offset;

        auto name = take_ident("parameter name");
        if (!name) {
            return std::nullopt;
        }
        p.name = std::move(*name);

        if (!expect(TokenKind::Colon, "':'")) {
            return std::nullopt;
        }

        auto ty = parse_type_name();
        if (!ty) {
            return std::nullopt;
        }
        p.ty = std::move(*ty);
        return p;
    }

    std::optional<std::string> parse_type_name() {
        if (consume(TokenKind::LParen)) {
            if (!expect(TokenKind::RParen, "')' after '(' in type")) {
                return std::nullopt;
            }
            return std::string("()");
        }

        auto name = take_ident("type name");
        if (!name) {
            return std::nullopt;
        }
        return name;
    }

    std::optional<Block> parse_block() {
        Block block;
        block.offset = peek().offset;

        if (!expect(TokenKind::LBrace, "'{'")) {
            return std::nullopt;
        }

        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            if (at(TokenKind::KwLet) || at(TokenKind::KwReturn)) {
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

            error(peek(), "expected ';' or '}' after expression");
            return std::nullopt;
        }

        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return block;
    }

    std::optional<StmtPtr> parse_stmt_keyword() {
        if (at(TokenKind::KwLet)) {
            return parse_let();
        }
        return parse_return();
    }

    std::optional<StmtPtr> parse_let() {
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
            auto ty = parse_type_name();
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

    std::optional<StmtPtr> parse_return() {
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

    std::optional<ExprPtr> parse_expr(bool allow_struct = true) { return parse_prec(1, allow_struct); }

    static int binding_power(TokenKind kind) {
        switch (kind) {
            case TokenKind::Equal:
                return 1;
            case TokenKind::Plus:
            case TokenKind::Minus:
                return 2;
            case TokenKind::Star:
            case TokenKind::Slash:
            case TokenKind::Percent:
                return 3;
            default:
                return -1;
        }
    }

    std::optional<ExprPtr> parse_prec(int min_bp, bool allow_struct = true) {
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

            auto rhs = parse_prec(bp + 1, allow_struct);
            if (!rhs) {
                return std::nullopt;
            }
            left = make_expr(op_off, ExprBinary{op, std::move(*left), std::move(*rhs)});
        }

        return left;
    }

    std::optional<ExprPtr> parse_unary(bool allow_struct = true) {
        if (!at(TokenKind::Minus)) {
            return parse_postfix(allow_struct);
        }

        const std::size_t off = peek().offset;
        advance();

        auto operand = parse_unary(allow_struct);
        if (!operand) {
            return std::nullopt;
        }
        return make_expr(off, ExprUnary{TokenKind::Minus, std::move(*operand)});
    }

    std::optional<ExprPtr> parse_postfix(bool allow_struct = true) {
        auto expr = parse_primary(allow_struct);
        if (!expr) {
            return std::nullopt;
        }

        while (true) {
            if (at(TokenKind::LParen)) {
                expr = parse_call(std::move(*expr));
                if (!expr) {
                    return std::nullopt;
                }
                continue;
            }
            if (at(TokenKind::Dot)) {
                expr = parse_field(std::move(*expr));
                if (!expr) {
                    return std::nullopt;
                }
                continue;
            }
            break;
        }
        return expr;
    }

    std::optional<ExprPtr> parse_field(ExprPtr base) {
        const std::size_t off = peek().offset;
        advance();

        auto name = take_ident("field name");
        if (!name) {
            return std::nullopt;
        }
        return make_expr(off, ExprField{std::move(base), std::move(*name)});
    }

    std::optional<ExprPtr> parse_call(ExprPtr callee) {
        const std::size_t off = peek().offset;
        advance();

        auto args = parse_arg_list();
        if (!args) {
            return std::nullopt;
        }
        return make_expr(off, ExprCall{std::move(callee), std::move(*args)});
    }

    std::optional<ExprPtr> parse_struct_lit(std::size_t off, std::vector<std::string> path) {
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
            lit.name = path.size() == 1 ? path[0] : path[0];
        }
        lit.path = std::move(path);
        lit.fields = std::move(fields);
        return make_expr(off, std::move(lit));
    }

    static bool is_type_suffix(std::string_view text) {
        return text == "i8" || text == "i16" || text == "i32" || text == "i64" || text == "u8" ||
               text == "u16" || text == "u32" || text == "u64" || text == "f32" || text == "f64" ||
               text == "byte";
    }

    std::optional<std::string> take_suffix() {
        if (!at(TokenKind::Ident) || !is_type_suffix(peek_text())) {
            return std::nullopt;
        }
        std::string suffix(peek_text());
        advance();
        return suffix;
    }

    std::optional<PatPtr> parse_pat() {
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

    std::optional<ExprPtr> parse_match() {
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

    std::optional<std::vector<ExprPtr>> parse_arg_list() {
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

    std::optional<ExprPtr> parse_primary(bool allow_struct = true) {
        if (at(TokenKind::KwMatch)) {
            return parse_match();
        }

        if (at(TokenKind::KwTrue) || at(TokenKind::KwFalse)) {
            const std::size_t off = peek().offset;
            const bool value = at(TokenKind::KwTrue);
            advance();
            return make_expr(off, LitBool{value});
        }

        if (at(TokenKind::String)) {
            const std::size_t off = peek().offset;
            std::string raw(peek_text());
            advance();
            return make_expr(off, LitString{std::move(raw)});
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
            if (allow_struct && at(TokenKind::LBrace)) {
                return parse_struct_lit(off, std::move(path));
            }
            if (path.size() == 1) {
                return make_expr(off, ExprIdent{std::move(path[0])});
            }
            return make_expr(off, ExprPath{std::move(path)});
        }

        if (consume(TokenKind::LParen)) {
            auto inner = parse_expr();
            if (!inner) {
                return std::nullopt;
            }
            if (!expect(TokenKind::RParen, "')'")) {
                return std::nullopt;
            }
            return inner;
        }

        error(peek(), std::string("expected expression, found ") + token_kind_name(peek().kind));
        return std::nullopt;
    }
};

}  // namespace

AstFile parse(const Source& src, const std::vector<Token>& tokens, DiagnosticEngine& diags) {
    return Parser{src, tokens, diags}.parse_file();
}

}  // namespace qpc

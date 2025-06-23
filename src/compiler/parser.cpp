#include "compiler/parser.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
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
               !at(TokenKind::KwVariant) && !at(TokenKind::KwExtern) && !at(TokenKind::KwLet) &&
               !at(TokenKind::KwMod) && !at(TokenKind::KwUse) && !at(TokenKind::KwTrait)) {
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

        if (at(TokenKind::KwEnum) ||
            (at(TokenKind::KwPub) && peek_n(1).kind == TokenKind::KwEnum)) {
            auto en = parse_c_enum();
            if (!en) {
                return false;
            }
            file.enums.push_back(std::move(*en));
            return true;
        }

        if (at(TokenKind::KwVariant) ||
            (at(TokenKind::KwPub) && peek_n(1).kind == TokenKind::KwVariant)) {
            auto var = parse_variant_type();
            if (!var) {
                return false;
            }
            file.variants.push_back(std::move(*var));
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

        if (at(TokenKind::KwExtern)) {
            return parse_extern_block(file);
        }

        if (at(TokenKind::KwMod) ||
            (at(TokenKind::KwPub) && peek_n(1).kind == TokenKind::KwMod)) {
            auto mod = parse_mod();
            if (!mod) {
                return false;
            }
            file.mods.push_back(std::move(*mod));
            return true;
        }

        if (at(TokenKind::KwUse)) {
            auto use = parse_use();
            if (!use) {
                return false;
            }
            file.uses.push_back(std::move(*use));
            return true;
        }

        if (at(TokenKind::KwTrait) ||
            (at(TokenKind::KwPub) && peek_n(1).kind == TokenKind::KwTrait)) {
            auto tr = parse_trait();
            if (!tr) {
                return false;
            }
            file.traits.push_back(std::move(*tr));
            return true;
        }

        if (at(TokenKind::KwLet) || (at(TokenKind::KwPub) && peek_n(1).kind == TokenKind::KwLet)) {
            auto st = parse_static();
            if (!st) {
                return false;
            }
            file.statics.push_back(std::move(*st));
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

    std::optional<StructDecl> parse_struct(bool in_extern = false) {
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
        st.is_extern = in_extern;

        if (in_extern) {
            if (!expect(TokenKind::Semicolon, "';' after opaque struct")) {
                return std::nullopt;
            }
            st.opaque = true;
            return st;
        }

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

        auto ty = parse_type();
        if (!ty) {
            return std::nullopt;
        }
        field.ty = std::move(*ty);
        return field;
    }

    std::optional<EnumDecl> parse_c_enum() {
        EnumDecl en;
        en.offset = peek().offset;
        en.pub = consume(TokenKind::KwPub);

        if (!expect(TokenKind::KwEnum, "'enum'")) {
            return std::nullopt;
        }

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

        std::int64_t next = 0;
        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            EnumMember member;
            member.offset = peek().offset;
            auto mname = take_ident("enum member");
            if (!mname) {
                return std::nullopt;
            }
            member.name = std::move(*mname);
            if (at(TokenKind::LBrace) || at(TokenKind::LParen)) {
                error(peek(), "enum members cannot have fields, use 'variant'");
                return std::nullopt;
            }
            if (consume(TokenKind::Equal)) {
                auto value = take_i64("enum discriminant");
                if (!value) {
                    return std::nullopt;
                }
                member.value = *value;
                next = *value + 1;
            } else {
                member.value = next;
                ++next;
            }
            en.members.push_back(std::move(member));
            consume(TokenKind::Comma);
        }

        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return en;
    }

    std::optional<VariantTypeDecl> parse_variant_type() {
        VariantTypeDecl en;
        en.offset = peek().offset;
        en.pub = consume(TokenKind::KwPub);

        if (!expect(TokenKind::KwVariant, "'variant'")) {
            return std::nullopt;
        }

        const std::size_t name_off = peek().offset;
        auto name = take_ident("variant name");
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
                auto ty = parse_type();
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

    std::optional<ImplDecl> parse_impl(bool prototype = false) {
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
            auto method = parse_fn(true, prototype);
            if (!method) {
                return std::nullopt;
            }
            if (prototype) {
                method->is_extern = true;
            }
            impl.methods.push_back(std::move(*method));
        }

        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return impl;
    }

    bool parse_extern_block(AstFile& file) {
        if (!expect(TokenKind::KwExtern, "'extern'")) {
            return false;
        }

        Abi abi = Abi::Qplus;
        if (at(TokenKind::String)) {
            if (peek_text() != "\"C\"") {
                error(peek(), "unknown ABI, expected \"C\"");
                return false;
            }
            abi = Abi::C;
            advance();
        }

        if (!expect(TokenKind::LBrace, "'{'")) {
            return false;
        }

        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            if (at(TokenKind::KwImpl)) {
                if (abi == Abi::C) {
                    error(peek(), "'extern \"C\"' can only declare functions");
                    return false;
                }
                auto impl = parse_impl(true);
                if (!impl) {
                    return false;
                }
                file.impls.push_back(std::move(*impl));
                continue;
            }

            if (at(TokenKind::KwStruct) ||
                (at(TokenKind::KwPub) && peek_n(1).kind == TokenKind::KwStruct)) {
                if (abi == Abi::C) {
                    error(peek(), "'extern \"C\"' can only declare functions");
                    return false;
                }
                auto st = parse_struct(true);
                if (!st) {
                    return false;
                }
                file.structs.push_back(std::move(*st));
                continue;
            }

            if (at(TokenKind::KwLet) || (at(TokenKind::KwPub) && peek_n(1).kind == TokenKind::KwLet)) {
                if (abi == Abi::C) {
                    error(peek(), "'extern \"C\"' can only declare functions");
                    return false;
                }
                auto st = parse_static(true);
                if (!st) {
                    return false;
                }
                file.statics.push_back(std::move(*st));
                continue;
            }

            auto fn = parse_fn(false, true);
            if (!fn) {
                return false;
            }
            fn->is_extern = true;
            fn->abi = abi;
            file.functions.push_back(std::move(*fn));
        }

        return expect(TokenKind::RBrace, "'}'");
    }

    std::optional<ModDecl> parse_mod() {
        ModDecl m;
        m.offset = peek().offset;
        m.pub = consume(TokenKind::KwPub);
        if (!expect(TokenKind::KwMod, "'mod'")) {
            return std::nullopt;
        }
        auto name = take_ident("module name");
        if (!name) {
            return std::nullopt;
        }
        m.name = std::move(*name);
        if (consume(TokenKind::Semicolon)) {
            m.file = true;
            m.body = std::make_unique<AstFile>();
            return m;
        }
        if (!expect(TokenKind::LBrace, "'{' or ';' after module name")) {
            return std::nullopt;
        }
        m.body = std::make_unique<AstFile>();
        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            if (!parse_item(*m.body)) {
                return std::nullopt;
            }
        }
        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return m;
    }

    std::optional<UseDecl> parse_use() {
        UseDecl u;
        u.offset = peek().offset;
        if (!expect(TokenKind::KwUse, "'use'")) {
            return std::nullopt;
        }
        while (true) {
            if (at(TokenKind::Star)) {
                u.glob = true;
                advance();
                break;
            }
            auto part = take_ident("path segment");
            if (!part) {
                return std::nullopt;
            }
            u.path.push_back(std::move(*part));
            if (!consume(TokenKind::ColonColon)) {
                break;
            }
        }
        if (!expect(TokenKind::Semicolon, "';'")) {
            return std::nullopt;
        }
        return u;
    }

    std::optional<TraitDecl> parse_trait() {
        TraitDecl tr;
        tr.offset = peek().offset;
        tr.pub = consume(TokenKind::KwPub);
        if (!expect(TokenKind::KwTrait, "'trait'")) {
            return std::nullopt;
        }
        auto name = take_ident("trait name");
        if (!name) {
            return std::nullopt;
        }
        tr.name = std::move(*name);
        if (!expect(TokenKind::LBrace, "'{'")) {
            return std::nullopt;
        }
        while (!at(TokenKind::Eof) && !at(TokenKind::RBrace)) {
            FnDecl tmp;
            auto method = parse_fn(true, true);
            if (!method) {
                return std::nullopt;
            }
            TraitMethod tm;
            tm.self_param = method->self_param;
            tm.name = std::move(method->name);
            tm.params = std::move(method->params);
            tm.return_ty = std::move(method->return_ty);
            tm.offset = method->offset;
            tr.methods.push_back(std::move(tm));
        }
        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return tr;
    }

    std::optional<StaticDecl> parse_static(bool is_extern = false) {
        StaticDecl st;
        st.offset = peek().offset;
        st.pub = consume(TokenKind::KwPub);
        if (!expect(TokenKind::KwLet, "'let'")) {
            return std::nullopt;
        }
        st.mut = consume(TokenKind::KwMut);
        auto name = take_ident("static name");
        if (!name) {
            return std::nullopt;
        }
        st.name = std::move(*name);
        if (consume(TokenKind::Colon)) {
            auto ty = parse_type();
            if (!ty) {
                return std::nullopt;
            }
            st.ty = std::move(*ty);
        }
        st.is_extern = is_extern;
        if (is_extern) {
            if (!st.ty) {
                error(peek(), "extern static requires a type annotation");
                return std::nullopt;
            }
            if (at(TokenKind::Equal)) {
                error(peek(), "extern static cannot have an initializer");
                return std::nullopt;
            }
            if (!expect(TokenKind::Semicolon, "';'")) {
                return std::nullopt;
            }
            return st;
        }
        if (!expect(TokenKind::Equal, "'='")) {
            return std::nullopt;
        }
        auto init = parse_expr();
        if (!init) {
            return std::nullopt;
        }
        st.init = std::move(*init);
        if (!expect(TokenKind::Semicolon, "';'")) {
            return std::nullopt;
        }
        return st;
    }

    std::optional<std::vector<TypeParam>> parse_type_params() {
        if (!expect(TokenKind::Lt, "'<'")) {
            return std::nullopt;
        }
        std::vector<TypeParam> params;
        if (at(TokenKind::Gt)) {
            advance();
            return params;
        }
        while (true) {
            TypeParam p;
            p.offset = peek().offset;
            auto name = take_ident("type parameter");
            if (!name) {
                return std::nullopt;
            }
            p.name = std::move(*name);
            if (consume(TokenKind::Colon)) {
                auto bound = take_ident("trait bound");
                if (!bound) {
                    return std::nullopt;
                }
                p.bound = std::move(*bound);
            }
            params.push_back(std::move(p));
            if (consume(TokenKind::Comma)) {
                continue;
            }
            break;
        }
        if (!expect(TokenKind::Gt, "'>'")) {
            return std::nullopt;
        }
        return params;
    }

    std::optional<std::vector<TypeExpr>> try_type_args() {
        const std::size_t saved = pos_;
        const std::size_t diags_at = diags_.size();
        if (!consume(TokenKind::Lt)) {
            return std::nullopt;
        }
        std::vector<TypeExpr> args;
        if (consume(TokenKind::Gt)) {
            return args;
        }
        while (true) {
            auto ty = parse_type();
            if (!ty) {
                pos_ = saved;
                diags_.truncate(diags_at);
                return std::nullopt;
            }
            args.push_back(std::move(*ty));
            if (consume(TokenKind::Comma)) {
                continue;
            }
            break;
        }
        if (!consume(TokenKind::Gt)) {
            pos_ = saved;
            diags_.truncate(diags_at);
            return std::nullopt;
        }
        return args;
    }

    std::optional<FnDecl> parse_fn(bool in_impl, bool prototype = false) {
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

        if (at(TokenKind::Lt)) {
            auto tparams = parse_type_params();
            if (!tparams) {
                return std::nullopt;
            }
            fn.type_params = std::move(*tparams);
        }

        auto params = parse_params(fn, in_impl);
        if (!params) {
            return std::nullopt;
        }
        fn.params = std::move(*params);

        if (consume(TokenKind::Arrow)) {
            auto ty = parse_type();
            if (!ty) {
                return std::nullopt;
            }
            fn.return_ty = std::move(*ty);
        }

        if (prototype) {
            if (at(TokenKind::LBrace)) {
                error(peek(), "extern function cannot have a body");
                return std::nullopt;
            }
            if (!expect(TokenKind::Semicolon, "';'")) {
                return std::nullopt;
            }
            return fn;
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

        auto ty = parse_type();
        if (!ty) {
            return std::nullopt;
        }
        p.ty = std::move(*ty);
        return p;
    }

    std::optional<std::int64_t> take_i64(const char* what) {
        if (!at(TokenKind::Int)) {
            error(peek(), std::string("expected ") + what);
            return std::nullopt;
        }
        std::string raw(peek_text());
        raw.erase(std::remove(raw.begin(), raw.end(), '_'), raw.end());
        try {
            const auto value = std::stoll(raw, nullptr, 0);
            advance();
            return value;
        } catch (...) {
            error(peek(), "invalid integer");
            advance();
            return std::nullopt;
        }
    }

    std::optional<TypeExpr> parse_type() {
        TypeExpr ty;
        ty.offset = peek().offset;

        if (consume(TokenKind::KwFn)) {
            if (!expect(TokenKind::LParen, "'(' after 'fn'")) {
                return std::nullopt;
            }
            ty.kind = TypeExpr::Kind::Fn;
            if (!at(TokenKind::RParen)) {
                while (true) {
                    auto arg = parse_type();
                    if (!arg) {
                        return std::nullopt;
                    }
                    ty.args.push_back(std::move(*arg));
                    if (!consume(TokenKind::Comma)) {
                        break;
                    }
                    if (at(TokenKind::RParen)) {
                        break;
                    }
                }
            }
            if (!expect(TokenKind::RParen, "')' in fn type")) {
                return std::nullopt;
            }
            if (!expect(TokenKind::Arrow, "'->' in fn type")) {
                return std::nullopt;
            }
            auto ret = parse_type();
            if (!ret) {
                return std::nullopt;
            }
            ty.args.push_back(std::move(*ret));
            return ty;
        }

        if (consume(TokenKind::LParen)) {
            if (!expect(TokenKind::RParen, "')' after '(' in type")) {
                return std::nullopt;
            }
            return TypeExpr::unit();
        }

        if (consume(TokenKind::LBracket)) {
            auto elem = parse_type();
            if (!elem) {
                return std::nullopt;
            }
            if (consume(TokenKind::Semicolon)) {
                auto n = take_i64("array length");
                if (!n || *n < 0) {
                    error(peek(), "array length must be a non-negative integer");
                    return std::nullopt;
                }
                if (!expect(TokenKind::RBracket, "']'")) {
                    return std::nullopt;
                }
                ty.kind = TypeExpr::Kind::Array;
                ty.array_len = static_cast<std::size_t>(*n);
                ty.args.push_back(std::move(*elem));
                return ty;
            }
            if (!expect(TokenKind::RBracket, "']'")) {
                return std::nullopt;
            }
            ty.kind = TypeExpr::Kind::List;
            ty.args.push_back(std::move(*elem));
            return ty;
        }

        if (consume(TokenKind::LBrace)) {
            auto key = parse_type();
            if (!key) {
                return std::nullopt;
            }
            if (!expect(TokenKind::Colon, "':'")) {
                return std::nullopt;
            }
            auto value = parse_type();
            if (!value) {
                return std::nullopt;
            }
            if (!expect(TokenKind::RBrace, "'}'")) {
                return std::nullopt;
            }
            ty.kind = TypeExpr::Kind::Dict;
            ty.args.push_back(std::move(*key));
            ty.args.push_back(std::move(*value));
            return ty;
        }

        auto name = take_ident("type name");
        if (!name) {
            return std::nullopt;
        }
        std::string full = std::move(*name);
        while (consume(TokenKind::ColonColon)) {
            auto part = take_ident("type path");
            if (!part) {
                return std::nullopt;
            }
            full += "::";
            full += *part;
        }
        return TypeExpr::named(std::move(full));
    }

    std::optional<Block> parse_block() {
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

    std::optional<StmtPtr> parse_stmt_keyword() {
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

    std::optional<StmtPtr> parse_while() {
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

    std::optional<StmtPtr> parse_for() {
        const std::size_t off = peek().offset;
        advance();
        auto name = take_ident("loop variable");
        if (!name) {
            return std::nullopt;
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
        return make_stmt(
            off, StmtFor{std::move(*name), std::move(*iter), std::make_unique<Block>(std::move(*body))});
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
            case TokenKind::PipePipe:
                return 2;
            case TokenKind::AmpAmp:
                return 3;
            case TokenKind::EqEq:
            case TokenKind::BangEq:
                return 4;
            case TokenKind::Lt:
            case TokenKind::Le:
            case TokenKind::Gt:
            case TokenKind::Ge:
                return 5;
            case TokenKind::DotDot:
                return 6;
            case TokenKind::Plus:
            case TokenKind::Minus:
                return 7;
            case TokenKind::Star:
            case TokenKind::Slash:
            case TokenKind::Percent:
                return 8;
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
            if (op == TokenKind::DotDot) {
                auto rhs = parse_prec(bp + 1, allow_struct);
                if (!rhs) {
                    return std::nullopt;
                }
                left = make_expr(op_off, ExprRange{std::move(*left), std::move(*rhs)});
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

    std::optional<ExprPtr> parse_postfix(bool allow_struct = true) {
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
                expr = parse_field(std::move(*expr));
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
            break;
        }
        return expr;
    }

    std::optional<ExprPtr> parse_index(ExprPtr base) {
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

    std::optional<ExprPtr> parse_field(ExprPtr base) {
        const std::size_t off = peek().offset;
        advance();

        auto name = take_ident("field name");
        if (!name) {
            return std::nullopt;
        }
        return make_expr(off, ExprField{std::move(base), std::move(*name)});
    }

    std::optional<ExprPtr> parse_call(ExprPtr callee, std::vector<TypeExpr> type_args) {
        const std::size_t off = peek().offset;
        advance();

        auto args = parse_arg_list();
        if (!args) {
            return std::nullopt;
        }
        return make_expr(off, ExprCall{std::move(callee), std::move(type_args), std::move(*args)});
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

    std::optional<ExprPtr> parse_closure() {
        const std::size_t off = peek().offset;
        if (!expect(TokenKind::Pipe, "'|'")) {
            return std::nullopt;
        }
        ExprClosure clo;
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

    std::optional<ExprPtr> parse_if() {
        const std::size_t off = peek().offset;
        if (!expect(TokenKind::KwIf, "'if'")) {
            return std::nullopt;
        }
        auto cond = parse_expr(false);
        if (!cond) {
            return std::nullopt;
        }
        auto then_block = parse_block();
        if (!then_block) {
            return std::nullopt;
        }
        ExprIf node;
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
        if (at(TokenKind::KwIf)) {
            return parse_if();
        }

        if (at(TokenKind::Pipe)) {
            return parse_closure();
        }

        if (at(TokenKind::PipePipe)) {
            const std::size_t off = peek().offset;
            advance();
            ExprClosure clo;
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

        if (at(TokenKind::LBracket)) {
            return parse_list_lit();
        }

        if (at(TokenKind::LBrace)) {
            return parse_dict_lit();
        }

        error(peek(), std::string("expected expression, found ") + token_kind_name(peek().kind));
        return std::nullopt;
    }

    std::optional<ExprPtr> parse_list_lit() {
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

    std::optional<ExprPtr> parse_dict_lit() {
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
};

}  // namespace

AstFile parse(const Source& src, const std::vector<Token>& tokens, DiagnosticEngine& diags) {
    return Parser{src, tokens, diags}.parse_file();
}

}  // namespace qpc

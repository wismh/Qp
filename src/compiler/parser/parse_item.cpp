#include "compiler/parser/parser_detail.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace qpc::detail {

bool Parser::parse_item(AstFile& file) {
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

        if (at(TokenKind::KwFrom)) {
            auto use = parse_from();
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

std::optional<std::string> Parser::take_ident(const char* what) {
        if (!at(TokenKind::Ident)) {
            error(peek(), std::string("expected ") + what);
            return std::nullopt;
        }

        std::string name(peek_text());
        advance();
        return name;
    }

std::optional<StructDecl> Parser::parse_struct(bool in_extern) {
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

        if (at(TokenKind::Lt)) {
            auto tparams = parse_type_params();
            if (!tparams) {
                return std::nullopt;
            }
            st.type_params = std::move(*tparams);
        }

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

std::optional<FieldDecl> Parser::parse_field() {
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

std::optional<EnumDecl> Parser::parse_c_enum() {
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

std::optional<VariantTypeDecl> Parser::parse_variant_type() {
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

std::optional<VariantDecl> Parser::parse_variant() {
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

std::optional<ImplDecl> Parser::parse_impl(bool prototype) {
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

        if (at(TokenKind::Lt)) {
            auto tparams = parse_type_params();
            if (!tparams) {
                return std::nullopt;
            }
            impl.type_params = std::move(*tparams);
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

bool Parser::parse_extern_block(AstFile& file) {
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

std::optional<ModDecl> Parser::parse_mod() {
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

std::optional<std::vector<std::string>> Parser::parse_use_brace_names() {
        if (!expect(TokenKind::LBrace, "'{'")) {
            return std::nullopt;
        }
        std::vector<std::string> names;
        std::unordered_set<std::string> seen;
        if (at(TokenKind::RBrace)) {
            error(peek(), "expected imported name");
            return std::nullopt;
        }
        while (true) {
            auto name = take_ident("imported name");
            if (!name) {
                return std::nullopt;
            }
            if (!seen.insert(*name).second) {
                error(peek(), "duplicate name '" + *name + "' in use list");
                return std::nullopt;
            }
            names.push_back(std::move(*name));
            if (!consume(TokenKind::Comma)) {
                break;
            }
            if (at(TokenKind::RBrace)) {
                break;
            }
        }
        if (!expect(TokenKind::RBrace, "'}'")) {
            return std::nullopt;
        }
        return names;
    }

std::optional<UseDecl> Parser::parse_use() {
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
            if (at(TokenKind::LBrace)) {
                if (u.path.empty()) {
                    error(peek(), "use list needs a path, like 'use math::{min, max}'");
                    return std::nullopt;
                }
                auto names = parse_use_brace_names();
                if (!names) {
                    return std::nullopt;
                }
                u.names = std::move(*names);
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

std::optional<UseDecl> Parser::parse_from() {
        UseDecl u;
        u.from_load = true;
        u.offset = peek().offset;
        if (!expect(TokenKind::KwFrom, "'from'")) {
            return std::nullopt;
        }
        auto name = take_ident("module name");
        if (!name) {
            return std::nullopt;
        }
        u.path.push_back(std::move(*name));
        if (!expect(TokenKind::KwUse, "'use'")) {
            return std::nullopt;
        }
        if (consume(TokenKind::Star)) {
            u.glob = true;
        } else if (at(TokenKind::LBrace)) {
            auto names = parse_use_brace_names();
            if (!names) {
                return std::nullopt;
            }
            u.names = std::move(*names);
        } else {
            error(peek(), "expected '*' or '{' after 'from ... use'");
            return std::nullopt;
        }
        if (!expect(TokenKind::Semicolon, "';'")) {
            return std::nullopt;
        }
        return u;
    }

std::optional<TraitDecl> Parser::parse_trait() {
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

std::optional<StaticDecl> Parser::parse_static(bool is_extern) {
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

std::optional<std::vector<TypeParam>> Parser::parse_type_params() {
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
            p.pack = consume(TokenKind::DotDotDot);
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
                if (params.back().pack) {
                    error(peek(), "type-parameter pack must be last");
                    return std::nullopt;
                }
                continue;
            }
            break;
        }
        if (!expect(TokenKind::Gt, "'>'")) {
            return std::nullopt;
        }
        return params;
    }

std::optional<std::vector<TypeExpr>> Parser::try_type_args() {
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

std::optional<FnDecl> Parser::parse_fn(bool in_impl, bool prototype) {
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

bool Parser::consume_self_param(FnDecl& fn) {
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

std::optional<std::vector<Param>> Parser::parse_params(FnDecl& fn, bool in_impl) {
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

            if (params.size() > 1 && params[params.size() - 2].pack) {
                error(peek(), "parameter pack must be last");
                return std::nullopt;
            }

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

std::optional<Param> Parser::parse_param() {
        Param p;
        p.offset = peek().offset;
        p.pack = consume(TokenKind::DotDotDot);
        p.mut = consume(TokenKind::KwMut);

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

std::optional<std::int64_t> Parser::take_i64(const char* what) {
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

std::optional<TypeExpr> Parser::parse_type() {
        auto ty = parse_bare_type();
        if (!ty) {
            return std::nullopt;
        }
        while (consume(TokenKind::Question)) {
            TypeExpr wrapped;
            wrapped.kind = TypeExpr::Kind::Nullable;
            wrapped.offset = ty->offset;
            wrapped.args.push_back(std::move(*ty));
            ty = std::move(wrapped);
        }
        return ty;
    }

std::optional<TypeExpr> Parser::parse_bare_type() {
        TypeExpr ty;
        ty.offset = peek().offset;

        if (consume(TokenKind::KwDyn)) {
            auto name = take_ident("trait name after 'dyn'");
            if (!name) {
                return std::nullopt;
            }
            std::string full = std::move(*name);
            while (consume(TokenKind::ColonColon)) {
                auto part = take_ident("trait path");
                if (!part) {
                    return std::nullopt;
                }
                full += "::";
                full += *part;
            }
            ty.kind = TypeExpr::Kind::Dyn;
            ty.name = std::move(full);
            return ty;
        }

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
            if (consume(TokenKind::RParen)) {
                return TypeExpr::unit();
            }
            auto first = parse_type();
            if (!first) {
                return std::nullopt;
            }
            if (consume(TokenKind::RParen)) {
                if (first->pack_expand) {
                    ty.kind = TypeExpr::Kind::Tuple;
                    ty.args.push_back(std::move(*first));
                    return ty;
                }
                return first;
            }
            if (!expect(TokenKind::Comma, "',' in tuple type")) {
                return std::nullopt;
            }
            ty.kind = TypeExpr::Kind::Tuple;
            ty.args.push_back(std::move(*first));
            while (!at(TokenKind::RParen) && !at(TokenKind::Eof)) {
                auto next = parse_type();
                if (!next) {
                    return std::nullopt;
                }
                ty.args.push_back(std::move(*next));
                if (!consume(TokenKind::Comma)) {
                    break;
                }
            }
            if (!expect(TokenKind::RParen, "')' in tuple type")) {
                return std::nullopt;
            }
            if (ty.args.size() < 2) {
                error(peek(), "tuple type needs at least two elements");
                return std::nullopt;
            }
            return ty;
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
        ty.name = std::move(full);
        if (auto targs = try_type_args()) {
            ty.args = std::move(*targs);
        }
        if (consume(TokenKind::DotDotDot)) {
            ty.pack_expand = true;
        }
        return ty;
    }

std::optional<ExprPtr> Parser::parse_field(ExprPtr base, bool null_safe) {
        const std::size_t off = peek().offset;
        advance();

        if (at(TokenKind::Int)) {
            std::string name(peek_text());
            advance();
            return make_expr(off, ExprField{std::move(base), std::move(name), null_safe});
        }

        auto name = take_ident("field name");
        if (!name) {
            return std::nullopt;
        }
        return make_expr(off, ExprField{std::move(base), std::move(*name), null_safe});
    }

}  // namespace qpc::detail

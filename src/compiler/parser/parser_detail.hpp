#pragma once

#include "compiler/ast.hpp"
#include "compiler/diagnostic.hpp"
#include "compiler/parser/parser_helpers.hpp"
#include "compiler/source.hpp"
#include "compiler/token.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace qpc::detail {

class Parser {
public:
    Parser(const Source& src, const std::vector<Token>& tokens, DiagnosticEngine& diags)
        : src_(src), tokens_(tokens), diags_(diags) {}


    AstFile parse_file();

private:
    const Source& src_;
    const std::vector<Token>& tokens_;
    DiagnosticEngine& diags_;
    std::size_t pos_ = 0;


    const Token& peek() const;


    const Token& peek_n(std::size_t n) const;


    bool at(TokenKind kind) const;


    std::string_view peek_text() const;


    const Token& advance();


    bool consume(TokenKind kind);


    bool expect(TokenKind kind, const char* what);


    void error(const Token& tok, std::string message);


    void recover_to_item();


    bool parse_item(AstFile& file);


    std::optional<std::string> take_ident(const char* what);


    std::optional<StructDecl> parse_struct(bool in_extern = false);


    std::optional<FieldDecl> parse_field();


    std::optional<EnumDecl> parse_c_enum();


    std::optional<VariantTypeDecl> parse_variant_type();


    std::optional<VariantDecl> parse_variant();


    std::optional<ImplDecl> parse_impl(bool prototype = false);


    bool parse_extern_block(AstFile& file);


    std::optional<ModDecl> parse_mod();


    std::optional<UseDecl> parse_use();


    std::optional<UseDecl> parse_from();


    std::optional<std::vector<std::string>> parse_use_brace_names();


    std::optional<TraitDecl> parse_trait();


    std::optional<StaticDecl> parse_static(bool is_extern = false);


    std::optional<std::vector<TypeParam>> parse_type_params();


    std::optional<std::vector<TypeExpr>> try_type_args();


    std::optional<FnDecl> parse_fn(bool in_impl, bool prototype = false);


    bool consume_self_param(FnDecl& fn);


    std::optional<std::vector<Param>> parse_params(FnDecl& fn, bool in_impl);


    std::optional<Param> parse_param();


    std::optional<std::int64_t> take_i64(const char* what);


    std::optional<TypeExpr> parse_type();


    std::optional<TypeExpr> parse_bare_type();


    std::optional<Block> parse_block();


    std::optional<StmtPtr> parse_stmt_keyword();


    std::optional<StmtPtr> parse_while();


    std::optional<StmtPtr> parse_for();


    std::optional<StmtPtr> parse_let();


    std::optional<StmtPtr> parse_return();


    std::optional<ExprPtr> parse_expr(bool allow_struct = true);


    static int binding_power(TokenKind kind);


    std::optional<ExprPtr> parse_prec(int min_bp, bool allow_struct = true);


    std::optional<ExprPtr> parse_unary(bool allow_struct = true);


    std::optional<ExprPtr> parse_postfix(bool allow_struct = true);


    std::optional<ExprPtr> parse_index(ExprPtr base);


    std::optional<ExprPtr> parse_field(ExprPtr base, bool null_safe);


    std::optional<ExprPtr> parse_call(ExprPtr callee, std::vector<TypeExpr> type_args);


    std::optional<ExprPtr> parse_struct_lit(std::size_t off, std::vector<std::string> path,
                                            std::vector<TypeExpr> type_args = {});


    std::optional<ExprPtr> parse_new();


    static bool is_type_suffix(std::string_view text);


    std::optional<std::string> take_suffix();


    std::optional<PatPtr> parse_pat();


    std::optional<ExprPtr> parse_closure(bool by_ref = false);


    std::optional<ExprPtr> parse_if();


    std::optional<ExprPtr> parse_match();


    std::optional<std::vector<ExprPtr>> parse_arg_list();


    std::optional<ExprPtr> parse_string_interp();


    std::optional<ExprPtr> parse_primary(bool allow_struct = true);


    std::optional<ExprPtr> parse_list_lit();


    std::optional<ExprPtr> parse_dict_lit();
};

}  // namespace qpc::detail

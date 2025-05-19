#pragma once

#include "compiler/token.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace qpc {

struct TypeExpr {
    enum class Kind { Named, Unit, List, Array, Dict };

    Kind kind = Kind::Named;
    std::string name;
    std::size_t array_len = 0;
    std::vector<TypeExpr> args;
    std::size_t offset = 0;

    static TypeExpr named(std::string n) {
        TypeExpr t;
        t.kind = Kind::Named;
        t.name = std::move(n);
        return t;
    }

    static TypeExpr unit() {
        TypeExpr t;
        t.kind = Kind::Unit;
        t.name = "()";
        return t;
    }
};

struct Expr;
struct Stmt;
struct Pat;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using PatPtr = std::unique_ptr<Pat>;

enum class SelfParam {
    None,
    Value,
    Mut,
};

struct LitInt {
    std::string raw;
    std::optional<std::string> suffix;
};

struct LitFloat {
    std::string raw;
    std::optional<std::string> suffix;
};

struct LitBool {
    bool value = false;
};

struct LitChar {
    std::string raw;
};

struct LitString {
    std::string raw;
};

struct ExprIdent {
    std::string name;
};

struct ExprPath {
    std::vector<std::string> segments;
};

struct ExprBinary {
    TokenKind op = TokenKind::Plus;
    ExprPtr lhs;
    ExprPtr rhs;
};

struct ExprUnary {
    TokenKind op = TokenKind::Minus;
    ExprPtr operand;
};

struct ExprCall {
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

struct ExprAssign {
    ExprPtr lhs;
    ExprPtr rhs;
};

struct ExprField {
    ExprPtr base;
    std::string name;
};

struct StructLitField {
    std::string name;
    ExprPtr value;
};

struct ExprStructLit {
    std::string name;
    std::vector<std::string> path;
    std::vector<StructLitField> fields;
};

struct MatchArm {
    PatPtr pat;
    ExprPtr body;
};

struct ExprMatch {
    ExprPtr scrutinee;
    std::vector<MatchArm> arms;
};

struct ExprIndex {
    ExprPtr base;
    ExprPtr index;
};

struct ExprListLit {
    std::vector<ExprPtr> elems;
};

struct ExprDictEntry {
    ExprPtr key;
    ExprPtr value;
};

struct ExprDictLit {
    std::vector<ExprDictEntry> entries;
};

struct Expr {
    std::size_t offset = 0;
    std::variant<LitInt, LitFloat, LitBool, LitChar, LitString, ExprIdent, ExprPath, ExprBinary,
                 ExprUnary, ExprCall, ExprAssign, ExprField, ExprIndex, ExprStructLit, ExprMatch,
                 ExprListLit, ExprDictLit>
        kind;
};

struct PatWild {};

struct PatIdent {
    std::string name;
};

struct PatVariant {
    std::vector<std::string> path;
    std::vector<std::string> fields;
    std::vector<PatPtr> args;
    bool tuple = false;
};

struct Pat {
    std::size_t offset = 0;
    std::variant<PatWild, PatIdent, PatVariant> kind;
};

struct StmtLet {
    bool mut = false;
    std::string name;
    std::optional<TypeExpr> ty;
    ExprPtr init;
};

struct StmtReturn {
    ExprPtr value;
};

struct StmtExpr {
    ExprPtr expr;
};

struct Stmt {
    std::size_t offset = 0;
    std::variant<StmtLet, StmtReturn, StmtExpr> kind;
};

struct Block {
    std::vector<StmtPtr> stmts;
    ExprPtr tail;
    std::size_t offset = 0;
};

struct Param {
    std::string name;
    TypeExpr ty;
    std::size_t offset = 0;
};

enum class Abi {
    Qplus,
    C,
};

struct FnDecl {
    bool pub = false;
    bool is_extern = false;
    Abi abi = Abi::Qplus;
    SelfParam self_param = SelfParam::None;
    std::string name;
    std::vector<Param> params;
    std::optional<TypeExpr> return_ty;
    Block body;
    std::size_t offset = 0;
};

struct FieldDecl {
    bool pub = false;
    bool mut = false;
    std::string name;
    TypeExpr ty;
    std::size_t offset = 0;
};

struct StructDecl {
    bool pub = false;
    std::string name;
    std::vector<FieldDecl> fields;
    std::size_t offset = 0;
};

struct VariantDecl {
    std::string name;
    std::vector<FieldDecl> fields;
    bool tuple = false;
    std::size_t offset = 0;
};

struct EnumMember {
    std::string name;
    std::optional<std::int64_t> value;
    std::size_t offset = 0;
};

struct EnumDecl {
    bool pub = false;
    std::string name;
    std::vector<EnumMember> members;
    std::size_t offset = 0;
};

struct VariantTypeDecl {
    bool pub = false;
    std::string name;
    std::vector<VariantDecl> variants;
    std::size_t offset = 0;
};

struct ImplDecl {
    std::optional<std::string> trait_name;
    std::string type_name;
    std::vector<FnDecl> methods;
    std::size_t offset = 0;
};

struct AstFile {
    std::vector<StructDecl> structs;
    std::vector<EnumDecl> enums;
    std::vector<VariantTypeDecl> variants;
    std::vector<ImplDecl> impls;
    std::vector<FnDecl> functions;
};

}  // namespace qpc

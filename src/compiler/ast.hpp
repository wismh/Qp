#pragma once

#include "compiler/token.hpp"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace qpc {

struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct LitInt {
    std::string raw;
};

struct LitFloat {
    std::string raw;
};

struct ExprIdent {
    std::string name;
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

struct Expr {
    std::size_t offset = 0;
    std::variant<LitInt, LitFloat, ExprIdent, ExprBinary, ExprUnary, ExprCall, ExprAssign> kind;
};

struct StmtLet {
    bool mut = false;
    std::string name;
    std::optional<std::string> ty;
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
    std::string ty;
    std::size_t offset = 0;
};

struct FnDecl {
    bool pub = false;
    std::string name;
    std::vector<Param> params;
    std::optional<std::string> return_ty;
    Block body;
    std::size_t offset = 0;
};

struct AstFile {
    std::vector<FnDecl> functions;
};

}  // namespace qpc

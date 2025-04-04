#pragma once

#include "compiler/type.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace qpc {

struct HirExpr;
struct HirStmt;
using HirExprPtr = std::unique_ptr<HirExpr>;
using HirStmtPtr = std::unique_ptr<HirStmt>;

enum class BinOp { Add, Sub, Mul, Div, Mod };
enum class UnOp { Neg };

struct HirLitInt {
    std::int32_t value = 0;
};

struct HirLitFloat {
    float value = 0.0f;
};

struct HirVar {
    std::string name;
};

struct HirBinary {
    BinOp op = BinOp::Add;
    HirExprPtr lhs;
    HirExprPtr rhs;
};

struct HirUnary {
    UnOp op = UnOp::Neg;
    HirExprPtr operand;
};

struct HirCall {
    std::string callee;
    std::vector<HirExprPtr> args;
};

struct HirAssign {
    std::string name;
    HirExprPtr value;
};

struct HirExpr {
    Type ty = Type::Unknown;
    std::size_t offset = 0;
    std::variant<HirLitInt, HirLitFloat, HirVar, HirBinary, HirUnary, HirCall, HirAssign> kind;
};

struct HirLet {
    bool mut = false;
    std::string name;
    Type ty = Type::Unknown;
    HirExprPtr init;
};

struct HirReturn {
    HirExprPtr value;
};

struct HirExprStmt {
    HirExprPtr expr;
};

struct HirStmt {
    std::size_t offset = 0;
    std::variant<HirLet, HirReturn, HirExprStmt> kind;
};

struct HirBlock {
    std::vector<HirStmtPtr> stmts;
    HirExprPtr tail;
    std::size_t offset = 0;
};

struct HirParam {
    std::string name;
    Type ty = Type::Unknown;
    std::size_t offset = 0;
};

struct HirFn {
    bool pub = false;
    std::string name;
    std::vector<HirParam> params;
    Type return_ty = Type::Unit;
    HirBlock body;
    std::size_t offset = 0;
};

struct HirModule {
    std::vector<HirFn> functions;
};

}  // namespace qpc

#pragma once

#include "compiler/type.hpp"

#include <cstdint>
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
enum class SelfKind { None, Value, Mut };

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

struct HirFieldAccess {
    HirExprPtr base;
    std::string name;
};

struct HirStructLitField {
    std::string name;
    HirExprPtr value;
};

struct HirStructLit {
    std::string name;
    std::vector<HirStructLitField> fields;
};

struct HirMethodCall {
    HirExprPtr receiver;
    std::string method;
    std::vector<HirExprPtr> args;
};

struct HirFieldAssign {
    HirExprPtr base;
    std::string field;
    HirExprPtr value;
};

struct HirExpr {
    Type ty;
    std::size_t offset = 0;
    std::variant<HirLitInt, HirLitFloat, HirVar, HirBinary, HirUnary, HirCall, HirAssign,
                 HirFieldAccess, HirStructLit, HirMethodCall, HirFieldAssign>
        kind;
};

struct HirLet {
    bool mut = false;
    std::string name;
    Type ty;
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
    Type ty;
    std::size_t offset = 0;
};

struct HirFn {
    bool pub = false;
    SelfKind self_kind = SelfKind::None;
    std::string self_ty;
    std::string name;
    std::vector<HirParam> params;
    Type return_ty = Type::unit();
    HirBlock body;
    std::size_t offset = 0;
};

struct HirField {
    bool mut = false;
    std::string name;
    Type ty;
    std::size_t offset = 0;
};

struct HirStruct {
    bool pub = false;
    std::string name;
    std::vector<HirField> fields;
    std::size_t offset = 0;
};

struct HirImpl {
    std::string type_name;
    std::vector<HirFn> methods;
    std::size_t offset = 0;
};

struct HirModule {
    std::vector<HirStruct> structs;
    std::vector<HirImpl> impls;
    std::vector<HirFn> functions;
};

}  // namespace qpc

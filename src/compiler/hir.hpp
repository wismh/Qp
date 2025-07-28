#pragma once

#include "compiler/type.hpp"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace qpc {

class Source;

struct HirExpr;
struct HirStmt;
struct HirPat;
using HirExprPtr = std::unique_ptr<HirExpr>;
using HirStmtPtr = std::unique_ptr<HirStmt>;
using HirPatPtr = std::unique_ptr<HirPat>;

enum class BinOp { Add, Sub, Mul, Div, Mod, Eq, Ne, Lt, Le, Gt, Ge, And, Or };
enum class UnOp { Neg, Not };
enum class SelfKind { None, Value, Mut };

struct HirParam {
    std::string name;
    Type ty;
    std::size_t offset = 0;
};

struct HirBlock {
    std::vector<HirStmtPtr> stmts;
    HirExprPtr tail;
    std::size_t offset = 0;
};

struct HirLitInt {
    std::int64_t value = 0;
    bool unsuffixed = true;
    Type ty = Type::i32();
};

struct HirLitFloat {
    double value = 0.0;
    bool unsuffixed = true;
    Type ty = Type::f32();
};

struct HirLitBool {
    bool value = false;
};

struct HirLitChar {
    char32_t value = 0;
};

struct HirLitString {
    std::string value;
};

struct HirLitNull {};

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
    HirExprPtr callee_expr;
    std::vector<Type> type_args;
    std::vector<HirExprPtr> args;
};

struct HirAssign {
    std::string name;
    HirExprPtr value;
};

struct HirFieldAccess {
    HirExprPtr base;
    std::string name;
    bool null_safe = false;
    bool take_addr = false;
};

struct HirStructLitField {
    std::string name;
    HirExprPtr value;
};

struct HirStructLit {
    std::string name;
    std::vector<HirStructLitField> fields;
};

struct HirEnumLit {
    std::string enum_name;
    std::string variant;
    bool tuple = false;
    std::vector<HirStructLitField> fields;
    std::vector<HirExprPtr> args;
};

struct HirMethodCall {
    HirExprPtr receiver;
    std::string method;
    std::vector<Type> type_args;
    std::vector<HirExprPtr> args;
    bool associated = false;
    bool null_safe = false;
    bool wrap_ret = false;
};

struct HirFieldAssign {
    HirExprPtr base;
    std::string field;
    HirExprPtr value;
};

struct HirPatWild {};

struct HirPatBinding {
    std::string name;
};

struct HirPatVariant {
    std::string enum_name;
    std::string variant;
    bool tuple = false;
    std::vector<std::string> fields;
    std::vector<HirPatPtr> args;
};

struct HirPat {
    std::size_t offset = 0;
    std::variant<HirPatWild, HirPatBinding, HirPatVariant> kind;
};

struct HirMatchArm {
    HirPatPtr pat;
    HirExprPtr body;
};

struct HirMatch {
    HirExprPtr scrutinee;
    std::vector<HirMatchArm> arms;
};

struct HirIndex {
    HirExprPtr base;
    HirExprPtr index;
};

struct HirIndexAssign {
    HirExprPtr base;
    HirExprPtr index;
    HirExprPtr value;
};

struct HirListLit {
    std::vector<HirExprPtr> elems;
    bool array = false;
};

struct HirDictLit {
    std::vector<std::pair<HirExprPtr, HirExprPtr>> entries;
};

struct HirIf {
    HirExprPtr cond;
    std::string let_name;
    std::vector<HirStmtPtr> then_stmts;
    HirExprPtr then_tail;
    HirExprPtr else_expr;
};

struct HirRange {
    HirExprPtr start;
    HirExprPtr end;
};

struct HirClosure {
    std::vector<HirParam> params;
    Type return_ty = Type::unknown();
    HirBlock body;
};

struct HirCast {
    HirExprPtr expr;
    Type ty;
};

struct HirUnwrap {
    HirExprPtr expr;
};

struct HirNew {
    std::string name;
    std::vector<HirStructLitField> fields;
};

struct HirCoalesce {
    HirExprPtr lhs;
    HirExprPtr rhs;
};

struct HirExpr {
    Type ty;
    std::size_t offset = 0;
    std::variant<HirLitInt, HirLitFloat, HirLitBool, HirLitChar, HirLitString, HirLitNull, HirVar, HirBinary,
                 HirUnary, HirCall, HirAssign, HirFieldAccess, HirIndex, HirStructLit, HirEnumLit,
                 HirMethodCall, HirFieldAssign, HirIndexAssign, HirMatch, HirListLit, HirDictLit,
                 HirIf, HirRange, HirClosure, HirCast, HirUnwrap, HirNew, HirCoalesce>
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

struct HirWhile {
    HirExprPtr cond;
    std::vector<HirStmtPtr> stmts;
    HirExprPtr tail;
};

struct HirFor {
    std::string name;
    HirExprPtr iter;
    std::vector<HirStmtPtr> stmts;
    HirExprPtr tail;
};

struct HirBreak {};
struct HirContinue {};

struct HirStmt {
    std::size_t offset = 0;
    std::variant<HirLet, HirReturn, HirExprStmt, HirWhile, HirFor, HirBreak, HirContinue> kind;
};

struct HirTypeParam {
    std::string name;
    std::optional<std::string> bound;
};

struct HirFn {
    bool pub = false;
    bool is_extern = false;
    bool c_abi = false;
    SelfKind self_kind = SelfKind::None;
    std::string self_ty;
    std::string name;
    std::vector<HirTypeParam> type_params;
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
    bool is_extern = false;
    bool opaque = false;
    std::string name;
    std::vector<HirField> fields;
    std::size_t offset = 0;
};

struct HirEnumVariant {
    std::string name;
    std::vector<HirField> fields;
    bool tuple = false;
    std::size_t offset = 0;
};

struct HirCEnumMember {
    std::string name;
    std::int64_t value = 0;
    std::size_t offset = 0;
};

struct HirCEnum {
    bool pub = false;
    std::string name;
    std::vector<HirCEnumMember> members;
    std::size_t offset = 0;
};

struct HirVariant {
    bool pub = false;
    std::string name;
    std::vector<HirEnumVariant> variants;
    std::size_t offset = 0;
};

struct HirImpl {
    std::optional<std::string> trait_name;
    std::string type_name;
    std::vector<HirFn> methods;
    std::size_t offset = 0;
};

struct HirStatic {
    bool pub = false;
    bool mut = false;
    bool is_extern = false;
    std::string name;
    Type ty;
    HirExprPtr init;
    std::size_t offset = 0;
};

struct HirTraitMethod {
    SelfKind self_kind = SelfKind::None;
    std::string name;
    std::vector<HirParam> params;
    Type return_ty = Type::unit();
};

struct HirTrait {
    bool pub = false;
    std::string name;
    std::vector<HirTraitMethod> methods;
    std::size_t offset = 0;
};

struct HirUse {
    std::vector<std::string> path;
    bool glob = false;
    std::size_t offset = 0;
};

struct HirModule {
    std::string name;
    const Source* source = nullptr;
    std::vector<HirUse> uses;
    std::vector<HirModule> mods;
    std::vector<HirStatic> statics;
    std::vector<HirTrait> traits;
    std::vector<HirStruct> structs;
    std::vector<HirCEnum> enums;
    std::vector<HirVariant> variants;
    std::vector<HirImpl> impls;
    std::vector<HirFn> functions;
};

}  // namespace qpc

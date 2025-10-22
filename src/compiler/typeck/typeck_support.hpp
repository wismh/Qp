#pragma once

#include "compiler/hir.hpp"
#include "compiler/type.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace qpc::detail {


struct Binding {
    Type ty;
    bool mut = false;
};

struct FnSig {
    std::vector<HirTypeParam> type_params;
    std::vector<Type> params;
    std::vector<char> param_mut;
    Type ret = Type::unit();
    std::size_t offset = 0;
    bool c_abi = false;
};

struct MethodSig {
    SelfKind self_kind = SelfKind::None;
    std::vector<HirTypeParam> type_params;
    std::vector<Type> params;
    std::vector<char> param_mut;
    Type ret = Type::unit();
    std::size_t offset = 0;
};

struct FieldInfo {
    bool mut = false;
    Type ty;
    std::size_t offset = 0;
};

struct StructInfo {
    std::vector<std::pair<std::string, FieldInfo>> fields;
    std::vector<HirTypeParam> type_params;
    std::size_t offset = 0;
    bool opaque = false;
};

struct VariantInfo {
    std::vector<std::pair<std::string, FieldInfo>> fields;
    bool tuple = false;
    std::size_t index = 0;
    std::size_t offset = 0;
};

struct CEnumMemberInfo {
    std::int64_t value = 0;
    std::size_t index = 0;
    std::size_t offset = 0;
};

struct CEnumInfo {
    std::vector<std::pair<std::string, CEnumMemberInfo>> members;
    std::size_t offset = 0;
};

struct EnumInfo {
    std::vector<std::pair<std::string, VariantInfo>> variants;
    std::size_t offset = 0;
};

inline const char* binop_trait(BinOp op) {
    switch (op) {
        case BinOp::Add:
            return "Add";
        case BinOp::Sub:
            return "Sub";
        case BinOp::Mul:
            return "Mul";
        case BinOp::Div:
            return "Div";
        case BinOp::Mod:
            return "Rem";
        case BinOp::Eq:
        case BinOp::Ne:
        case BinOp::Lt:
        case BinOp::Le:
        case BinOp::Gt:
        case BinOp::Ge:
        case BinOp::And:
        case BinOp::Or:
            return "";
    }
    return "Add";
}

inline bool is_eq_op(BinOp op) { return op == BinOp::Eq || op == BinOp::Ne; }

inline bool is_ord_op(BinOp op) {
    return op == BinOp::Lt || op == BinOp::Le || op == BinOp::Gt || op == BinOp::Ge;
}

inline bool is_logic_op(BinOp op) { return op == BinOp::And || op == BinOp::Or; }

inline bool is_unary_math(std::string_view name) {
    return name == "sin" || name == "cos" || name == "tan" || name == "asin" || name == "acos" ||
           name == "atan" || name == "sqrt" || name == "abs" || name == "floor" || name == "ceil" ||
           name == "exp" || name == "ln" || name == "log2";
}

inline bool is_binary_math(std::string_view name) {
    return name == "atan2" || name == "fmod" || name == "pow";
}

inline bool is_math_builtin(std::string_view name) {
    return is_unary_math(name) || is_binary_math(name);
}

inline bool is_op_trait(std::string_view name) {
    return name == "Add" || name == "Sub" || name == "Mul" || name == "Div" || name == "Rem" ||
           name == "Neg";
}

}  // namespace qpc::detail

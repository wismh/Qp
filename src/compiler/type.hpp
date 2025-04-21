#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace qpc {

enum class TypeKind {
    Unknown,
    Error,
    Unit,
    I32,
    F32,
    Named,
};

struct Type {
    TypeKind kind = TypeKind::Unknown;
    std::string name;

    static Type unknown() { return {}; }
    static Type error() { return {TypeKind::Error, {}}; }
    static Type unit() { return {TypeKind::Unit, {}}; }
    static Type i32() { return {TypeKind::I32, {}}; }
    static Type f32() { return {TypeKind::F32, {}}; }
    static Type named(std::string n) { return {TypeKind::Named, std::move(n)}; }

    friend bool operator==(const Type& a, const Type& b) {
        return a.kind == b.kind && a.name == b.name;
    }

    friend bool operator!=(const Type& a, const Type& b) { return !(a == b); }
};

inline std::string type_name(const Type& ty) {
    switch (ty.kind) {
        case TypeKind::Unknown:
            return "<unknown>";
        case TypeKind::Error:
            return "<error>";
        case TypeKind::Unit:
            return "()";
        case TypeKind::I32:
            return "i32";
        case TypeKind::F32:
            return "f32";
        case TypeKind::Named:
            return ty.name;
    }
    return "<invalid>";
}

inline std::string cpp_type_name(const Type& ty) {
    switch (ty.kind) {
        case TypeKind::Unit:
            return "void";
        case TypeKind::I32:
            return "std::int32_t";
        case TypeKind::F32:
            return "float";
        case TypeKind::Named:
            return ty.name;
        default:
            return "void";
    }
}

inline Type type_from_name(std::string_view name) {
    if (name == "i32") {
        return Type::i32();
    }
    if (name == "f32") {
        return Type::f32();
    }
    if (name == "()" || name.empty()) {
        return Type::unit();
    }
    return Type::named(std::string(name));
}

}  // namespace qpc

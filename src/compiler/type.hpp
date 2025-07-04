#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace qpc {

enum class TypeKind {
    Unknown,
    Error,
    Unit,
    Bool,
    Char,
    String,
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,
    Named,
    List,
    Array,
    Dict,
    Nullable,
};

struct Type {
    TypeKind kind = TypeKind::Unknown;
    std::string name;
    std::size_t size = 0;
    std::vector<Type> args;

    static Type unknown() { return {}; }
    static Type error() { return {TypeKind::Error, {}}; }
    static Type unit() { return {TypeKind::Unit, {}}; }
    static Type boolean() { return {TypeKind::Bool, {}}; }
    static Type char_() { return {TypeKind::Char, {}}; }
    static Type string() { return {TypeKind::String, {}}; }
    static Type i8() { return {TypeKind::I8, {}}; }
    static Type i16() { return {TypeKind::I16, {}}; }
    static Type i32() { return {TypeKind::I32, {}}; }
    static Type i64() { return {TypeKind::I64, {}}; }
    static Type u8() { return {TypeKind::U8, {}}; }
    static Type u16() { return {TypeKind::U16, {}}; }
    static Type u32() { return {TypeKind::U32, {}}; }
    static Type u64() { return {TypeKind::U64, {}}; }
    static Type f32() { return {TypeKind::F32, {}}; }
    static Type f64() { return {TypeKind::F64, {}}; }
    static Type named(std::string n) { return {TypeKind::Named, std::move(n)}; }

    static Type list(Type elem) {
        Type t;
        t.kind = TypeKind::List;
        t.args.push_back(std::move(elem));
        return t;
    }

    static Type array(Type elem, std::size_t n) {
        Type t;
        t.kind = TypeKind::Array;
        t.size = n;
        t.args.push_back(std::move(elem));
        return t;
    }

    static Type dict(Type key, Type value) {
        Type t;
        t.kind = TypeKind::Dict;
        t.args.push_back(std::move(key));
        t.args.push_back(std::move(value));
        return t;
    }

    static Type nullable(Type inner) {
        Type t;
        t.kind = TypeKind::Nullable;
        t.args.push_back(std::move(inner));
        return t;
    }

    const Type& elem() const { return args.front(); }
    const Type& key() const { return args.front(); }
    const Type& value() const { return args.back(); }

    friend bool operator==(const Type& a, const Type& b) {
        return a.kind == b.kind && a.name == b.name && a.size == b.size && a.args == b.args;
    }

    friend bool operator!=(const Type& a, const Type& b) { return !(a == b); }
};

inline bool is_signed_int(const Type& ty) {
    return ty.kind == TypeKind::I8 || ty.kind == TypeKind::I16 || ty.kind == TypeKind::I32 ||
           ty.kind == TypeKind::I64;
}

inline bool is_unsigned_int(const Type& ty) {
    return ty.kind == TypeKind::U8 || ty.kind == TypeKind::U16 || ty.kind == TypeKind::U32 ||
           ty.kind == TypeKind::U64;
}

inline bool is_int(const Type& ty) { return is_signed_int(ty) || is_unsigned_int(ty); }

inline bool is_float(const Type& ty) { return ty.kind == TypeKind::F32 || ty.kind == TypeKind::F64; }

inline bool is_numeric(const Type& ty) { return is_int(ty) || is_float(ty); }

inline bool is_c_abi_type(const Type& ty) {
    return ty.kind == TypeKind::Unit || ty.kind == TypeKind::Bool || is_numeric(ty);
}

inline int int_bit_width(const Type& ty) {
    switch (ty.kind) {
        case TypeKind::I8:
        case TypeKind::U8:
            return 8;
        case TypeKind::I16:
        case TypeKind::U16:
            return 16;
        case TypeKind::I32:
        case TypeKind::U32:
            return 32;
        case TypeKind::I64:
        case TypeKind::U64:
            return 64;
        default:
            return 0;
    }
}

inline bool int_fits(std::int64_t value, const Type& ty) {
    switch (ty.kind) {
        case TypeKind::I8:
            return value >= -128 && value <= 127;
        case TypeKind::I16:
            return value >= -32768 && value <= 32767;
        case TypeKind::I32:
            return value >= -2147483647LL - 1 && value <= 2147483647LL;
        case TypeKind::I64:
            return true;
        case TypeKind::U8:
            return value >= 0 && value <= 255;
        case TypeKind::U16:
            return value >= 0 && value <= 65535;
        case TypeKind::U32:
            return value >= 0 && value <= 4294967295LL;
        case TypeKind::U64:
            return value >= 0;
        default:
            return false;
    }
}

inline std::string type_name(const Type& ty) {
    switch (ty.kind) {
        case TypeKind::Unknown:
            return "<unknown>";
        case TypeKind::Error:
            return "<error>";
        case TypeKind::Unit:
            return "()";
        case TypeKind::Bool:
            return "bool";
        case TypeKind::Char:
            return "char";
        case TypeKind::String:
            return "string";
        case TypeKind::I8:
            return "i8";
        case TypeKind::I16:
            return "i16";
        case TypeKind::I32:
            return "i32";
        case TypeKind::I64:
            return "i64";
        case TypeKind::U8:
            return "u8";
        case TypeKind::U16:
            return "u16";
        case TypeKind::U32:
            return "u32";
        case TypeKind::U64:
            return "u64";
        case TypeKind::F32:
            return "f32";
        case TypeKind::F64:
            return "f64";
        case TypeKind::Named:
            return ty.name;
        case TypeKind::List:
            return "[" + type_name(ty.elem()) + "]";
        case TypeKind::Array:
            return "[" + type_name(ty.elem()) + "; " + std::to_string(ty.size) + "]";
        case TypeKind::Dict:
            return "{" + type_name(ty.key()) + ": " + type_name(ty.value()) + "}";
        case TypeKind::Nullable:
            return type_name(ty.elem()) + "?";
    }
    return "<invalid>";
}

inline std::string cpp_type_name(const Type& ty) {
    switch (ty.kind) {
        case TypeKind::Unit:
            return "void";
        case TypeKind::Bool:
            return "bool";
        case TypeKind::Char:
            return "char32_t";
        case TypeKind::String:
            return "String";
        case TypeKind::I8:
            return "std::int8_t";
        case TypeKind::I16:
            return "std::int16_t";
        case TypeKind::I32:
            return "std::int32_t";
        case TypeKind::I64:
            return "std::int64_t";
        case TypeKind::U8:
            return "std::uint8_t";
        case TypeKind::U16:
            return "std::uint16_t";
        case TypeKind::U32:
            return "std::uint32_t";
        case TypeKind::U64:
            return "std::uint64_t";
        case TypeKind::F32:
            return "float";
        case TypeKind::F64:
            return "double";
        case TypeKind::Named:
            return ty.name;
        case TypeKind::List:
            return "List<" + cpp_type_name(ty.elem()) + ">";
        case TypeKind::Array:
            return "Array<" + cpp_type_name(ty.elem()) + ", " + std::to_string(ty.size) + ">";
        case TypeKind::Dict:
            return "Dict<" + cpp_type_name(ty.key()) + ", " + cpp_type_name(ty.value()) + ">";
        case TypeKind::Nullable:
            return cpp_type_name(ty.elem()) + "*";
        default:
            return "void";
    }
}

inline Type type_from_name(std::string_view name) {
    if (name == "bool") {
        return Type::boolean();
    }
    if (name == "char") {
        return Type::char_();
    }
    if (name == "string") {
        return Type::string();
    }
    if (name == "i8") {
        return Type::i8();
    }
    if (name == "i16") {
        return Type::i16();
    }
    if (name == "i32") {
        return Type::i32();
    }
    if (name == "i64") {
        return Type::i64();
    }
    if (name == "u8" || name == "byte") {
        return Type::u8();
    }
    if (name == "u16") {
        return Type::u16();
    }
    if (name == "u32") {
        return Type::u32();
    }
    if (name == "u64") {
        return Type::u64();
    }
    if (name == "f32") {
        return Type::f32();
    }
    if (name == "f64") {
        return Type::f64();
    }
    if (name == "()" || name.empty()) {
        return Type::unit();
    }
    return Type::named(std::string(name));
}

inline Type type_from_suffix(std::string_view suffix) { return type_from_name(suffix); }

}  // namespace qpc

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace qpc {

enum class Type {
    Unknown,
    Error,
    Unit,
    I32,
    F32,
};

inline std::string_view type_name(Type ty) {
    switch (ty) {
        case Type::Unknown:
            return "<unknown>";
        case Type::Error:
            return "<error>";
        case Type::Unit:
            return "()";
        case Type::I32:
            return "i32";
        case Type::F32:
            return "f32";
    }
    return "<invalid>";
}

inline const char* cpp_type_name(Type ty) {
    switch (ty) {
        case Type::Unit:
            return "void";
        case Type::I32:
            return "std::int32_t";
        case Type::F32:
            return "float";
        default:
            return "void";
    }
}

inline Type type_from_name(std::string_view name) {
    if (name == "i32") {
        return Type::I32;
    }
    if (name == "f32") {
        return Type::F32;
    }
    if (name == "()" || name.empty()) {
        return Type::Unit;
    }
    return Type::Error;
}

}  // namespace qpc

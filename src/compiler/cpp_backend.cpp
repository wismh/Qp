#include "compiler/cpp_backend.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace qpc {
namespace {

std::string line_path(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (char c : path) {
        out.push_back(c == '\\' ? '/' : c);
    }
    return out;
}

const char* binop_spelling(BinOp op) {
    switch (op) {
        case BinOp::Add:
            return "+";
        case BinOp::Sub:
            return "-";
        case BinOp::Mul:
            return "*";
        case BinOp::Div:
            return "/";
        case BinOp::Mod:
            return "%";
    }
    return "+";
}

const char* trait_operator(const std::string& trait) {
    if (trait == "Add") {
        return "+";
    }
    if (trait == "Sub" || trait == "Neg") {
        return "-";
    }
    if (trait == "Mul") {
        return "*";
    }
    if (trait == "Div") {
        return "/";
    }
    if (trait == "Rem") {
        return "%";
    }
    return "+";
}

std::string cpp_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\0':
                out += "\\0";
                break;
            default:
                out.push_back(static_cast<char>(c));
                break;
        }
    }
    return out;
}

void emit_expr(std::ostringstream& out, const HirExpr& expr);

void emit_comma_list(std::ostringstream& out, const std::vector<HirExprPtr>& args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        emit_expr(out, *args[i]);
    }
}

void emit_receiver(std::ostringstream& out, const HirExpr& expr) {
    const bool wrap = !std::holds_alternative<HirVar>(expr.kind);
    if (wrap) {
        out << '(';
    }
    emit_expr(out, expr);
    if (wrap) {
        out << ')';
    }
}

void emit_params(std::ostringstream& out, const std::vector<HirParam>& params) {
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << cpp_type_name(params[i].ty) << ' ' << params[i].name;
    }
}

void emit_int_lit(std::ostringstream& out, const HirLitInt& lit) {
    switch (lit.ty.kind) {
        case TypeKind::U8:
        case TypeKind::U16:
        case TypeKind::U32:
        case TypeKind::U64:
            out << "static_cast<" << cpp_type_name(lit.ty) << ">(" << static_cast<unsigned long long>(lit.value)
                << "ull)";
            break;
        case TypeKind::I64:
            out << lit.value << "ll";
            break;
        case TypeKind::I8:
        case TypeKind::I16:
            out << "static_cast<" << cpp_type_name(lit.ty) << ">(" << lit.value << ")";
            break;
        default:
            out << lit.value;
            break;
    }
}

void emit_expr(std::ostringstream& out, const HirExpr& expr) {
    std::visit(
        [&](auto&& kind) {
            using K = std::decay_t<decltype(kind)>;
            if constexpr (std::is_same_v<K, HirLitInt>) {
                emit_int_lit(out, kind);
            } else if constexpr (std::is_same_v<K, HirLitFloat>) {
                if (kind.ty.kind == TypeKind::F64) {
                    out << std::format("{}", kind.value);
                } else {
                    out << std::format("{}f", kind.value);
                }
            } else if constexpr (std::is_same_v<K, HirLitBool>) {
                out << (kind.value ? "true" : "false");
            } else if constexpr (std::is_same_v<K, HirLitChar>) {
                out << "char32_t(" << static_cast<std::uint32_t>(kind.value) << ")";
            } else if constexpr (std::is_same_v<K, HirLitString>) {
                out << "String(\"" << cpp_escape(kind.value) << "\")";
            } else if constexpr (std::is_same_v<K, HirVar>) {
                out << kind.name;
            } else if constexpr (std::is_same_v<K, HirBinary>) {
                out << '(';
                emit_expr(out, *kind.lhs);
                out << ' ' << binop_spelling(kind.op) << ' ';
                emit_expr(out, *kind.rhs);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirUnary>) {
                out << "(-";
                emit_expr(out, *kind.operand);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirCall>) {
                out << kind.callee << '(';
                emit_comma_list(out, kind.args);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirAssign>) {
                out << '(' << kind.name << " = ";
                emit_expr(out, *kind.value);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirFieldAccess>) {
                emit_receiver(out, *kind.base);
                out << '.' << kind.name;
            } else if constexpr (std::is_same_v<K, HirStructLit>) {
                out << kind.name << '{';
                for (std::size_t i = 0; i < kind.fields.size(); ++i) {
                    if (i != 0) {
                        out << ", ";
                    }
                    out << '.' << kind.fields[i].name << " = ";
                    emit_expr(out, *kind.fields[i].value);
                }
                out << '}';
            } else if constexpr (std::is_same_v<K, HirEnumLit>) {
                out << kind.enum_name << '{' << kind.enum_name << "::" << kind.variant << '{';
                if (kind.tuple) {
                    for (std::size_t i = 0; i < kind.args.size(); ++i) {
                        if (i != 0) {
                            out << ", ";
                        }
                        out << "._" << i << " = ";
                        emit_expr(out, *kind.args[i]);
                    }
                } else {
                    for (std::size_t i = 0; i < kind.fields.size(); ++i) {
                        if (i != 0) {
                            out << ", ";
                        }
                        out << '.' << kind.fields[i].name << " = ";
                        emit_expr(out, *kind.fields[i].value);
                    }
                }
                out << "}}";
            } else if constexpr (std::is_same_v<K, HirMethodCall>) {
                emit_receiver(out, *kind.receiver);
                out << '.' << kind.method << '(';
                emit_comma_list(out, kind.args);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirFieldAssign>) {
                out << '(';
                emit_receiver(out, *kind.base);
                out << '.' << kind.field << " = ";
                emit_expr(out, *kind.value);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirMatch>) {
                const std::string scrut_ty =
                    kind.scrutinee->ty.kind == TypeKind::Named ? kind.scrutinee->ty.name : std::string{};
                out << "([&]() {\n";
                out << "        auto __s = ";
                emit_expr(out, *kind.scrutinee);
                out << ";\n";
                for (const auto& arm : kind.arms) {
                    if (const auto* var = std::get_if<HirPatVariant>(&arm.pat->kind)) {
                        const std::string en = var->enum_name.empty() ? scrut_ty : var->enum_name;
                        const bool binds = !var->fields.empty() || !var->args.empty();
                        if (binds) {
                            out << "        if (auto* __v = std::get_if<" << en << "::" << var->variant
                                << ">(&__s.data)) {\n";
                            for (const auto& field : var->fields) {
                                out << "            const auto " << field << " = __v->" << field << ";\n";
                            }
                            for (std::size_t a = 0; a < var->args.size(); ++a) {
                                if (const auto* bind = std::get_if<HirPatBinding>(&var->args[a]->kind)) {
                                    out << "            const auto " << bind->name << " = __v->_" << a
                                        << ";\n";
                                }
                            }
                        } else {
                            out << "        if (std::holds_alternative<" << en << "::" << var->variant
                                << ">(__s.data)) {\n";
                        }
                        out << "            return ";
                        emit_expr(out, *arm.body);
                        out << ";\n        }\n";
                    } else if (const auto* bind = std::get_if<HirPatBinding>(&arm.pat->kind)) {
                        out << "        const auto " << bind->name << " = __s;\n";
                        out << "        return ";
                        emit_expr(out, *arm.body);
                        out << ";\n";
                    } else {
                        out << "        return ";
                        emit_expr(out, *arm.body);
                        out << ";\n";
                    }
                }
                out << "        std::abort();\n    }())";
            }
        },
        expr.kind);
}

void emit_stmt(std::ostringstream& out, const HirStmt& stmt) {
    std::visit(
        [&](auto&& kind) {
            using K = std::decay_t<decltype(kind)>;
            if constexpr (std::is_same_v<K, HirLet>) {
                out << "    " << cpp_type_name(kind.ty) << ' ' << kind.name << " = ";
                emit_expr(out, *kind.init);
                out << ";\n";
            } else if constexpr (std::is_same_v<K, HirReturn>) {
                out << "    return";
                if (kind.value) {
                    out << ' ';
                    emit_expr(out, *kind.value);
                }
                out << ";\n";
            } else if constexpr (std::is_same_v<K, HirExprStmt>) {
                out << "    ";
                emit_expr(out, *kind.expr);
                out << ";\n";
            }
        },
        stmt.kind);
}

void emit_fn_body(std::ostringstream& out, const HirFn& fn, bool operator_self = false) {
    out << " {\n";
    if (!operator_self) {
        if (fn.self_kind == SelfKind::Value) {
            out << "    " << cpp_type_name(Type::named(fn.self_ty)) << " self = *this;\n";
        } else if (fn.self_kind == SelfKind::Mut) {
            out << "    " << cpp_type_name(Type::named(fn.self_ty)) << "& self = *this;\n";
        }
    }
    for (const auto& stmt : fn.body.stmts) {
        emit_stmt(out, *stmt);
    }
    if (fn.body.tail) {
        out << "    return ";
        emit_expr(out, *fn.body.tail);
        out << ";\n";
    }
    out << "}\n\n";
}

void emit_free_signature(std::ostringstream& out, const HirFn& fn) {
    out << cpp_type_name(fn.return_ty) << ' ' << fn.name << '(';
    emit_params(out, fn.params);
    out << ')';
}

void emit_method_signature(std::ostringstream& out, const HirFn& fn, bool qualified) {
    out << cpp_type_name(fn.return_ty) << ' ';
    if (qualified) {
        out << fn.self_ty << "::";
    }
    out << fn.name << '(';
    emit_params(out, fn.params);
    out << ')';
    if (fn.self_kind == SelfKind::Value) {
        out << " const";
    }
}

void emit_operator_signature(std::ostringstream& out, const HirImpl& impl, const HirFn& fn) {
    out << cpp_type_name(fn.return_ty) << " operator" << trait_operator(*impl.trait_name) << '(';
    out << impl.type_name << " self";
    for (const auto& p : fn.params) {
        out << ", " << cpp_type_name(p.ty) << ' ' << p.name;
    }
    out << ')';
}

std::unordered_map<std::string, std::vector<const HirFn*>> methods_by_type(const HirModule& mod) {
    std::unordered_map<std::string, std::vector<const HirFn*>> out;
    for (const auto& impl : mod.impls) {
        if (impl.trait_name) {
            continue;
        }
        for (const auto& method : impl.methods) {
            out[impl.type_name].push_back(&method);
        }
    }
    return out;
}

void emit_enum(std::ostringstream& header, const HirEnum& en) {
    header << "struct " << en.name << " {\n";
    for (const auto& variant : en.variants) {
        header << "    struct " << variant.name << " {";
        if (variant.fields.empty()) {
            header << "};\n";
            continue;
        }
        header << "\n";
        for (const auto& field : variant.fields) {
            header << "        " << cpp_type_name(field.ty) << ' ' << field.name << ";\n";
        }
        header << "    };\n";
    }
    header << "    std::variant<";
    for (std::size_t i = 0; i < en.variants.size(); ++i) {
        if (i != 0) {
            header << ", ";
        }
        header << en.variants[i].name;
    }
    header << "> data;\n";
    header << "};\n\n";
}

std::string emit_header(const HirModule& mod) {
    const auto methods = methods_by_type(mod);
    std::ostringstream header;
    header << "#pragma once\n\n";
    header << "#include <cstdint>\n";
    header << "#include <cstdlib>\n";
    header << "#include <string>\n";
    header << "#include <variant>\n\n";
    header << "namespace qplus {\n\n";
    header << "using String = std::string;\n\n";

    for (const auto& en : mod.enums) {
        emit_enum(header, en);
    }

    for (const auto& st : mod.structs) {
        header << "struct " << st.name << " {\n";
        for (const auto& field : st.fields) {
            header << "    " << cpp_type_name(field.ty) << ' ' << field.name << ";\n";
        }
        auto it = methods.find(st.name);
        if (it != methods.end()) {
            if (!st.fields.empty() && !it->second.empty()) {
                header << '\n';
            }
            for (const HirFn* method : it->second) {
                header << "    ";
                emit_method_signature(header, *method, false);
                header << ";\n";
            }
        }
        header << "};\n\n";
    }

    for (const auto& impl : mod.impls) {
        if (!impl.trait_name) {
            continue;
        }
        for (const auto& method : impl.methods) {
            emit_operator_signature(header, impl, method);
            header << ";\n";
        }
    }

    for (const auto& fn : mod.functions) {
        emit_free_signature(header, fn);
        header << ";\n";
    }
    if (!mod.functions.empty()) {
        header << '\n';
    }
    header << "}  // namespace qplus\n";
    return header.str();
}

std::string emit_source(const Source& src, const HirModule& mod, std::string_view header_name) {
    const std::string qp_path = line_path(src.path());
    std::ostringstream source;
    source << "#include \"" << header_name << "\"\n\n";
    source << "namespace qplus {\n\n";

    for (const auto& impl : mod.impls) {
        for (const auto& method : impl.methods) {
            const auto loc = src.location(method.offset);
            source << "#line " << loc.line << " \"" << qp_path << "\"\n";
            if (impl.trait_name) {
                emit_operator_signature(source, impl, method);
                emit_fn_body(source, method, true);
            } else {
                emit_method_signature(source, method, true);
                emit_fn_body(source, method);
            }
        }
    }

    for (const auto& fn : mod.functions) {
        const auto loc = src.location(fn.offset);
        source << "#line " << loc.line << " \"" << qp_path << "\"\n";
        emit_free_signature(source, fn);
        emit_fn_body(source, fn);
    }

    source << "}  // namespace qplus\n";
    return source.str();
}

}  // namespace

CppOutput emit_cpp(const Source& src, const HirModule& mod) {
    CppOutput out;
    const std::filesystem::path path(src.path());
    out.stem = path.stem().string();
    if (out.stem.empty()) {
        out.stem = "module";
    }

    const std::string header_name = out.stem + ".h";
    out.header = emit_header(mod);
    out.source = emit_source(src, mod, header_name);
    return out;
}

}  // namespace qpc

#include "compiler/cpp_backend.hpp"

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

void emit_expr(std::ostringstream& out, const HirExpr& expr) {
    std::visit(
        [&](auto&& kind) {
            using K = std::decay_t<decltype(kind)>;
            if constexpr (std::is_same_v<K, HirLitInt>) {
                out << kind.value;
            } else if constexpr (std::is_same_v<K, HirLitFloat>) {
                out << std::format("{}f", kind.value);
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

void emit_fn_body(std::ostringstream& out, const HirFn& fn) {
    out << " {\n";
    if (fn.self_kind == SelfKind::Value) {
        out << "    " << cpp_type_name(Type::named(fn.self_ty)) << " self = *this;\n";
    } else if (fn.self_kind == SelfKind::Mut) {
        out << "    " << cpp_type_name(Type::named(fn.self_ty)) << "& self = *this;\n";
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

std::unordered_map<std::string, std::vector<const HirFn*>> methods_by_type(const HirModule& mod) {
    std::unordered_map<std::string, std::vector<const HirFn*>> out;
    for (const auto& impl : mod.impls) {
        for (const auto& method : impl.methods) {
            out[impl.type_name].push_back(&method);
        }
    }
    return out;
}

std::string emit_header(const HirModule& mod) {
    const auto methods = methods_by_type(mod);
    std::ostringstream header;
    header << "#pragma once\n\n";
    header << "#include <cstdint>\n\n";
    header << "namespace qplus {\n\n";

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
            emit_method_signature(source, method, true);
            emit_fn_body(source, method);
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

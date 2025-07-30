#include "compiler/cpp_backend.hpp"

#include <cstdint>
#include <filesystem>
#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace qpc {
namespace {

const HirModule* g_mod = nullptr;
int g_if_tmp = 0;
std::ostringstream* g_preamble = nullptr;
std::unordered_map<const HirExpr*, std::string> g_try_tmps;

void emit_expr(std::ostringstream& out, const HirExpr& expr);
void emit_stmt(std::ostringstream& out, const HirStmt& stmt);
void emit_try_setup(std::ostringstream& out, const HirExpr& expr);

void emit_expr(std::ostringstream& out, const HirExpr& expr);
void emit_stmt(std::ostringstream& out, const HirStmt& stmt);

bool is_c_enum_in(const HirModule& mod, const std::string& name) {
    for (const auto& en : mod.enums) {
        if (en.name == name) {
            return true;
        }
    }
    for (const auto& child : mod.mods) {
        if (is_c_enum_in(child, name)) {
            return true;
        }
    }
    return false;
}

bool is_c_enum_name(const std::string& name) {
    return g_mod && is_c_enum_in(*g_mod, name);
}

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
        case BinOp::Eq:
            return "==";
        case BinOp::Ne:
            return "!=";
        case BinOp::Lt:
            return "<";
        case BinOp::Le:
            return "<=";
        case BinOp::Gt:
            return ">";
        case BinOp::Ge:
            return ">=";
        case BinOp::And:
            return "&&";
        case BinOp::Or:
            return "||";
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
void emit_stmt(std::ostringstream& out, const HirStmt& stmt);

bool is_generic(const HirFn& fn) { return !fn.type_params.empty(); }

std::string join_path(const std::vector<std::string>& path) {
    std::string out;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i != 0) {
            out += "::";
        }
        out += path[i];
    }
    return out;
}

void emit_type_args(std::ostringstream& out, const std::vector<Type>& args) {
    if (args.empty()) {
        return;
    }
    out << '<';
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << cpp_type_name(args[i]);
    }
    out << '>';
}

void emit_template_head(std::ostringstream& out, const std::vector<HirTypeParam>& tps,
                        std::string_view indent = "") {
    if (tps.empty()) {
        return;
    }
    out << indent << "template <";
    for (std::size_t i = 0; i < tps.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << "typename " << tps[i].name;
    }
    out << ">\n";
}

void emit_uses(std::ostringstream& out, const HirModule& mod) {
    for (const auto& u : mod.uses) {
        if (u.glob) {
            out << "using namespace " << join_path(u.path) << ";\n";
        } else if (!u.path.empty()) {
            out << "using " << join_path(u.path) << ";\n";
        }
    }
    if (!mod.uses.empty()) {
        out << '\n';
    }
}

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

bool is_true_lit(const HirExpr& expr) {
    const auto* b = std::get_if<HirLitBool>(&expr.kind);
    return b && b->value;
}

bool is_unitish(const Type& ty) {
    return ty.kind == TypeKind::Unit || ty.kind == TypeKind::Error || ty.kind == TypeKind::Unknown ||
           ty.kind == TypeKind::Never;
}

enum class IfSink { Stmt, Return, Assign };

void emit_if_stmt(std::ostringstream& out, const HirIf& iff, IfSink sink, const std::string& dest,
                  bool chained = false);

void emit_if_value(std::ostringstream& out, const HirExpr& expr, IfSink sink, const std::string& dest) {
    if (const auto* inner = std::get_if<HirIf>(&expr.kind)) {
        emit_if_stmt(out, *inner, sink, dest, false);
        return;
    }
    emit_try_setup(out, expr);
    if (sink == IfSink::Return && !is_unitish(expr.ty)) {
        out << "        return ";
        emit_expr(out, expr);
        out << ";\n";
        return;
    }
    if (sink == IfSink::Assign && !is_unitish(expr.ty)) {
        out << "        " << dest << " = ";
        emit_expr(out, expr);
        out << ";\n";
        return;
    }
    out << "        ";
    emit_expr(out, expr);
    out << ";\n";
}

void emit_if_stmt(std::ostringstream& out, const HirIf& iff, IfSink sink, const std::string& dest,
                  bool chained) {
    emit_try_setup(out, *iff.cond);
    if (!chained) {
        out << "    ";
    }
    out << "if (";
    emit_expr(out, *iff.cond);
    out << ") {\n";
    for (const auto& stmt : iff.then_stmts) {
        emit_stmt(out, *stmt);
    }
    if (iff.then_tail) {
        emit_if_value(out, *iff.then_tail, sink, dest);
    }
    out << "    }";
    if (iff.else_expr) {
        if (const auto* ei = std::get_if<HirIf>(&iff.else_expr->kind); ei && !is_true_lit(*ei->cond)) {
            out << " else ";
            emit_if_stmt(out, *ei, sink, dest, true);
        } else {
            out << " else {\n";
            if (const auto* always = std::get_if<HirIf>(&iff.else_expr->kind); always && is_true_lit(*always->cond)) {
                for (const auto& stmt : always->then_stmts) {
                    emit_stmt(out, *stmt);
                }
                if (always->then_tail) {
                    emit_if_value(out, *always->then_tail, sink, dest);
                }
            } else {
                emit_if_value(out, *iff.else_expr, sink, dest);
            }
            out << "    }";
        }
    }
    if (!chained) {
        out << "\n";
    }
}

void emit_expr(std::ostringstream& out, const HirExpr& expr) {
    std::visit(
        [&](auto&& kind) {
            using K = std::decay_t<decltype(kind)>;
            if constexpr (std::is_same_v<K, HirLitInt>) {
                emit_int_lit(out, kind);
            } else if constexpr (std::is_same_v<K, HirLitFloat>) {
                std::string text = std::format("{}", kind.value);
                if (text.find('.') == std::string::npos && text.find('e') == std::string::npos &&
                    text.find('E') == std::string::npos) {
                    text += ".0";
                }
                if (kind.ty.kind != TypeKind::F64) {
                    text += 'f';
                }
                out << text;
            } else if constexpr (std::is_same_v<K, HirLitBool>) {
                out << (kind.value ? "true" : "false");
            } else if constexpr (std::is_same_v<K, HirLitChar>) {
                out << "char32_t(" << static_cast<std::uint32_t>(kind.value) << ")";
            } else if constexpr (std::is_same_v<K, HirLitString>) {
                out << "String(\"" << cpp_escape(kind.value) << "\")";
            } else if constexpr (std::is_same_v<K, HirLitNull>) {
                out << "nullptr";
            } else if constexpr (std::is_same_v<K, HirVar>) {
                out << kind.name;
            } else if constexpr (std::is_same_v<K, HirBinary>) {
                out << '(';
                emit_expr(out, *kind.lhs);
                out << ' ' << binop_spelling(kind.op) << ' ';
                emit_expr(out, *kind.rhs);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirUnary>) {
                out << '(' << (kind.op == UnOp::Not ? '!' : '-');
                emit_expr(out, *kind.operand);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirCall>) {
                if (kind.callee_expr) {
                    out << '(';
                    emit_expr(out, *kind.callee_expr);
                    out << ')';
                } else {
                    out << kind.callee;
                    emit_type_args(out, kind.type_args);
                }
                out << '(';
                emit_comma_list(out, kind.args);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirAssign>) {
                out << '(' << kind.name << " = ";
                emit_expr(out, *kind.value);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirFieldAccess>) {
                emit_receiver(out, *kind.base);
                out << '.' << kind.name;
            } else if constexpr (std::is_same_v<K, HirIndex>) {
                emit_receiver(out, *kind.base);
                if (kind.base->ty.kind == TypeKind::Dict) {
                    out << ".at(";
                    emit_expr(out, *kind.index);
                    out << ')';
                } else {
                    out << '[';
                    emit_expr(out, *kind.index);
                    out << ']';
                }
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
                if (is_c_enum_name(kind.enum_name)) {
                    out << kind.enum_name << "::" << kind.variant;
                } else {
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
                }
            } else if constexpr (std::is_same_v<K, HirMethodCall>) {
                emit_receiver(out, *kind.receiver);
                out << (kind.associated ? "::" : ".") << kind.method;
                emit_type_args(out, kind.type_args);
                out << '(';
                emit_comma_list(out, kind.args);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirFieldAssign>) {
                out << '(';
                emit_receiver(out, *kind.base);
                out << '.' << kind.field << " = ";
                emit_expr(out, *kind.value);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirIndexAssign>) {
                out << '(';
                emit_receiver(out, *kind.base);
                out << '[';
                emit_expr(out, *kind.index);
                out << "] = ";
                emit_expr(out, *kind.value);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirListLit>) {
                out << cpp_type_name(expr.ty) << '{';
                emit_comma_list(out, kind.elems);
                out << '}';
            } else if constexpr (std::is_same_v<K, HirDictLit>) {
                out << cpp_type_name(expr.ty) << '{';
                for (std::size_t i = 0; i < kind.entries.size(); ++i) {
                    if (i != 0) {
                        out << ", ";
                    }
                    out << '{';
                    emit_expr(out, *kind.entries[i].first);
                    out << ", ";
                    emit_expr(out, *kind.entries[i].second);
                    out << '}';
                }
                out << '}';
            } else if constexpr (std::is_same_v<K, HirMatch>) {
                const std::string scrut_ty =
                    kind.scrutinee->ty.kind == TypeKind::Named ? kind.scrutinee->ty.name : std::string{};
                const bool c_enum = is_c_enum_name(scrut_ty);
                out << "([&]() {\n";
                out << "        auto __s = ";
                emit_expr(out, *kind.scrutinee);
                out << ";\n";
                if (c_enum) {
                    out << "        switch (__s) {\n";
                }
                for (const auto& arm : kind.arms) {
                    if (const auto* var = std::get_if<HirPatVariant>(&arm.pat->kind)) {
                        const std::string en = var->enum_name.empty() ? scrut_ty : var->enum_name;
                        if (c_enum) {
                            out << "        case " << en << "::" << var->variant << ": return ";
                            emit_expr(out, *arm.body);
                            out << ";\n";
                            continue;
                        }
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
                        if (c_enum) {
                            out << "        default: {\n            const auto " << bind->name
                                << " = __s;\n            return ";
                            emit_expr(out, *arm.body);
                            out << ";\n        }\n";
                        } else {
                            out << "        const auto " << bind->name << " = __s;\n";
                            out << "        return ";
                            emit_expr(out, *arm.body);
                            out << ";\n";
                        }
                    } else if (c_enum) {
                        out << "        default: return ";
                        emit_expr(out, *arm.body);
                        out << ";\n";
                    } else {
                        out << "        return ";
                        emit_expr(out, *arm.body);
                        out << ";\n";
                    }
                }
                if (c_enum) {
                    out << "        }\n";
                }
                out << "        std::abort();\n    }())";
            } else if constexpr (std::is_same_v<K, HirIf>) {
                if (!g_preamble) {
                    emit_if_stmt(out, kind, is_unitish(expr.ty) ? IfSink::Stmt : IfSink::Return, {});
                } else if (is_unitish(expr.ty)) {
                    emit_if_stmt(*g_preamble, kind, IfSink::Stmt, {});
                } else {
                    const std::string tmp = "__qif" + std::to_string(++g_if_tmp);
                    *g_preamble << "    " << cpp_type_name(expr.ty) << ' ' << tmp << ";\n";
                    emit_if_stmt(*g_preamble, kind, IfSink::Assign, tmp, false);
                    out << tmp;
                }
            } else if constexpr (std::is_same_v<K, HirRange>) {
                emit_expr(out, *kind.start);
            } else if constexpr (std::is_same_v<K, HirClosure>) {
                out << cpp_type_name(expr.ty) << "([=](";
                emit_params(out, kind.params);
                out << ')';
                if (kind.return_ty != Type::unit()) {
                    out << " -> " << cpp_type_name(kind.return_ty);
                }
                out << " {\n";
                for (const auto& stmt : kind.body.stmts) {
                    emit_stmt(out, *stmt);
                }
                if (kind.body.tail) {
                    if (kind.return_ty == Type::unit() || kind.body.tail->ty == Type::unit()) {
                        out << "        ";
                        emit_expr(out, *kind.body.tail);
                        out << ";\n";
                    } else {
                        out << "        return ";
                        emit_expr(out, *kind.body.tail);
                        out << ";\n";
                    }
                }
                out << "    })";
            } else if constexpr (std::is_same_v<K, HirCast>) {
                out << "static_cast<" << cpp_type_name(expr.ty) << ">(";
                emit_expr(out, *kind.expr);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirUnwrap>) {
                out << "unwrap(";
                emit_expr(out, *kind.expr);
                out << ')';
            } else if constexpr (std::is_same_v<K, HirTry>) {
                auto it = g_try_tmps.find(&expr);
                if (it != g_try_tmps.end()) {
                    out << "(*" << it->second << ")";
                } else {
                    out << "unwrap(";
                    emit_expr(out, *kind.expr);
                    out << ')';
                }
            }
        },
        expr.kind);
}

void emit_try_setup(std::ostringstream& out, const HirExpr& expr) {
    if (g_try_tmps.contains(&expr)) {
        return;
    }
    std::visit(
        [&](auto&& kind) {
            using K = std::decay_t<decltype(kind)>;
            if constexpr (std::is_same_v<K, HirTry>) {
                emit_try_setup(out, *kind.expr);
                const std::string tmp = "__qt" + std::to_string(++g_if_tmp);
                g_try_tmps[&expr] = tmp;
                out << "    " << cpp_type_name(kind.expr->ty) << ' ' << tmp << " = ";
                emit_expr(out, *kind.expr);
                out << ";\n    if (" << tmp << " == nullptr) {\n        return nullptr;\n    }\n";
            } else if constexpr (std::is_same_v<K, HirBinary>) {
                emit_try_setup(out, *kind.lhs);
                emit_try_setup(out, *kind.rhs);
            } else if constexpr (std::is_same_v<K, HirUnary>) {
                emit_try_setup(out, *kind.operand);
            } else if constexpr (std::is_same_v<K, HirCall>) {
                if (kind.callee_expr) {
                    emit_try_setup(out, *kind.callee_expr);
                }
                for (const auto& arg : kind.args) {
                    emit_try_setup(out, *arg);
                }
            } else if constexpr (std::is_same_v<K, HirAssign>) {
                emit_try_setup(out, *kind.value);
            } else if constexpr (std::is_same_v<K, HirFieldAccess>) {
                emit_try_setup(out, *kind.base);
            } else if constexpr (std::is_same_v<K, HirIndex>) {
                emit_try_setup(out, *kind.base);
                emit_try_setup(out, *kind.index);
            } else if constexpr (std::is_same_v<K, HirStructLit>) {
                for (const auto& field : kind.fields) {
                    emit_try_setup(out, *field.value);
                }
            } else if constexpr (std::is_same_v<K, HirEnumLit>) {
                for (const auto& field : kind.fields) {
                    emit_try_setup(out, *field.value);
                }
                for (const auto& arg : kind.args) {
                    emit_try_setup(out, *arg);
                }
            } else if constexpr (std::is_same_v<K, HirMethodCall>) {
                emit_try_setup(out, *kind.receiver);
                for (const auto& arg : kind.args) {
                    emit_try_setup(out, *arg);
                }
            } else if constexpr (std::is_same_v<K, HirFieldAssign>) {
                emit_try_setup(out, *kind.base);
                emit_try_setup(out, *kind.value);
            } else if constexpr (std::is_same_v<K, HirIndexAssign>) {
                emit_try_setup(out, *kind.base);
                emit_try_setup(out, *kind.index);
                emit_try_setup(out, *kind.value);
            } else if constexpr (std::is_same_v<K, HirListLit>) {
                for (const auto& elem : kind.elems) {
                    emit_try_setup(out, *elem);
                }
            } else if constexpr (std::is_same_v<K, HirDictLit>) {
                for (const auto& entry : kind.entries) {
                    emit_try_setup(out, *entry.first);
                    emit_try_setup(out, *entry.second);
                }
            } else if constexpr (std::is_same_v<K, HirCast>) {
                emit_try_setup(out, *kind.expr);
            } else if constexpr (std::is_same_v<K, HirUnwrap>) {
                emit_try_setup(out, *kind.expr);
            } else if constexpr (std::is_same_v<K, HirRange>) {
                emit_try_setup(out, *kind.start);
                emit_try_setup(out, *kind.end);
            } else if constexpr (std::is_same_v<K, HirIf>) {
                emit_try_setup(out, *kind.cond);
            } else if constexpr (std::is_same_v<K, HirMatch>) {
                emit_try_setup(out, *kind.scrutinee);
            }
        },
        expr.kind);
}

void emit_stmt(std::ostringstream& out, const HirStmt& stmt) {
    std::visit(
        [&](auto&& kind) {
            using K = std::decay_t<decltype(kind)>;
            if constexpr (std::is_same_v<K, HirLet>) {
                if (const auto* iff = std::get_if<HirIf>(&kind.init->kind)) {
                    out << "    " << cpp_type_name(kind.ty) << ' ' << kind.name << ";\n";
                    emit_if_stmt(out, *iff, IfSink::Assign, kind.name, false);
                } else {
                    emit_try_setup(out, *kind.init);
                    out << "    " << cpp_type_name(kind.ty) << ' ' << kind.name << " = ";
                    emit_expr(out, *kind.init);
                    out << ";\n";
                }
            } else if constexpr (std::is_same_v<K, HirReturn>) {
                if (kind.value) {
                    if (const auto* iff = std::get_if<HirIf>(&kind.value->kind)) {
                        emit_if_stmt(out, *iff, IfSink::Return, {}, false);
                    } else {
                        emit_try_setup(out, *kind.value);
                        out << "    return ";
                        emit_expr(out, *kind.value);
                        out << ";\n";
                    }
                } else {
                    out << "    return;\n";
                }
            } else if constexpr (std::is_same_v<K, HirExprStmt>) {
                if (const auto* iff = std::get_if<HirIf>(&kind.expr->kind)) {
                    emit_if_stmt(out, *iff, is_unitish(kind.expr->ty) ? IfSink::Stmt : IfSink::Return, {},
                                 false);
                } else {
                    emit_try_setup(out, *kind.expr);
                    out << "    ";
                    emit_expr(out, *kind.expr);
                    out << ";\n";
                }
            } else if constexpr (std::is_same_v<K, HirWhile>) {
                emit_try_setup(out, *kind.cond);
                out << "    while (";
                emit_expr(out, *kind.cond);
                out << ") {\n";
                for (const auto& inner : kind.stmts) {
                    emit_stmt(out, *inner);
                }
                if (kind.tail) {
                    out << "    ";
                    emit_expr(out, *kind.tail);
                    out << ";\n";
                }
                out << "    }\n";
            } else if constexpr (std::is_same_v<K, HirFor>) {
                if (const auto* range = std::get_if<HirRange>(&kind.iter->kind)) {
                    out << "    for (" << cpp_type_name(kind.iter->ty) << ' ' << kind.name << " = ";
                    emit_expr(out, *range->start);
                    out << "; " << kind.name << " < ";
                    emit_expr(out, *range->end);
                    out << "; ++" << kind.name << ") {\n";
                } else {
                    out << "    for (const auto& " << kind.name << " : ";
                    emit_expr(out, *kind.iter);
                    out << ") {\n";
                }
                for (const auto& inner : kind.stmts) {
                    emit_stmt(out, *inner);
                }
                if (kind.tail) {
                    out << "    ";
                    emit_expr(out, *kind.tail);
                    out << ";\n";
                }
                out << "    }\n";
            } else if constexpr (std::is_same_v<K, HirBreak>) {
                out << "    break;\n";
            } else if constexpr (std::is_same_v<K, HirContinue>) {
                out << "    continue;\n";
            }
        },
        stmt.kind);
}

void emit_fn_body(std::ostringstream& out, const HirFn& fn, bool operator_self = false) {
    g_preamble = &out;
    g_if_tmp = 0;
    g_try_tmps.clear();
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
        if (const auto* iff = std::get_if<HirIf>(&fn.body.tail->kind)) {
            const bool unit_tail = fn.return_ty == Type::unit() || is_unitish(fn.body.tail->ty);
            emit_if_stmt(out, *iff, unit_tail ? IfSink::Stmt : IfSink::Return, {}, false);
        } else {
            emit_try_setup(out, *fn.body.tail);
            const bool unit_tail = fn.return_ty == Type::unit() || fn.body.tail->ty == Type::unit();
            if (unit_tail) {
                out << "    ";
                emit_expr(out, *fn.body.tail);
                out << ";\n";
            } else {
                out << "    return ";
                emit_expr(out, *fn.body.tail);
                out << ";\n";
            }
        }
    }
    out << "}\n\n";
    g_preamble = nullptr;
}

void emit_free_signature(std::ostringstream& out, const HirFn& fn) {
    out << cpp_type_name(fn.return_ty) << ' ' << fn.name << '(';
    emit_params(out, fn.params);
    out << ')';
}

void emit_method_signature(std::ostringstream& out, const HirFn& fn, bool qualified) {
    if (!qualified && fn.self_kind == SelfKind::None) {
        out << "static ";
    }
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

void collect_methods_by_type(const HirModule& mod,
                             std::unordered_map<std::string, std::vector<const HirFn*>>& out) {
    for (const auto& impl : mod.impls) {
        if (impl.trait_name) {
            continue;
        }
        for (const auto& method : impl.methods) {
            out[impl.type_name].push_back(&method);
        }
    }
    for (const auto& child : mod.mods) {
        collect_methods_by_type(child, out);
    }
}

std::unordered_map<std::string, std::vector<const HirFn*>> methods_by_type(const HirModule& mod) {
    std::unordered_map<std::string, std::vector<const HirFn*>> out;
    collect_methods_by_type(mod, out);
    return out;
}

void emit_c_enum(std::ostringstream& header, const HirCEnum& en) {
    header << "enum class " << en.name << " : std::int32_t {\n";
    for (std::size_t i = 0; i < en.members.size(); ++i) {
        header << "    " << en.members[i].name << " = " << en.members[i].value;
        if (i + 1 != en.members.size()) {
            header << ',';
        }
        header << '\n';
    }
    header << "};\n\n";
}

void emit_variant(std::ostringstream& header, const HirVariant& en) {
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

void emit_structs(std::ostringstream& header, const HirModule& mod,
                  const std::unordered_map<std::string, std::vector<const HirFn*>>& methods) {
    for (const auto& st : mod.structs) {
        if (st.opaque) {
            continue;
        }
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
                emit_template_head(header, method->type_params, "    ");
                header << "    ";
                emit_method_signature(header, *method, false);
                if (is_generic(*method) && !method->is_extern) {
                    emit_fn_body(header, *method);
                } else {
                    header << ";\n";
                }
            }
        }
        header << "};\n\n";
    }
}

void emit_module_header(std::ostringstream& header, const HirModule& mod,
                        const std::unordered_map<std::string, std::vector<const HirFn*>>& methods) {
    for (const auto& en : mod.enums) {
        emit_c_enum(header, en);
    }
    for (const auto& var : mod.variants) {
        emit_variant(header, var);
    }
    emit_structs(header, mod, methods);

    for (const auto& child : mod.mods) {
        header << "namespace " << child.name << " {\n\n";
        emit_module_header(header, child, methods);
        header << "}  // namespace " << child.name << "\n\n";
    }

    emit_uses(header, mod);

    for (const auto& st : mod.statics) {
        if (st.is_extern) {
            header << "extern ";
            if (!st.mut) {
                header << "const ";
            }
            header << cpp_type_name(st.ty) << ' ' << st.name << ";\n";
            continue;
        }
        header << "inline ";
        if (!st.mut) {
            header << "const ";
        }
        header << cpp_type_name(st.ty) << ' ' << st.name << " = ";
        emit_expr(header, *st.init);
        header << ";\n";
    }
    if (!mod.statics.empty()) {
        header << '\n';
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
        if (fn.c_abi) {
            continue;
        }
        emit_template_head(header, fn.type_params);
        emit_free_signature(header, fn);
        if (is_generic(fn) && !fn.is_extern) {
            emit_fn_body(header, fn);
        } else {
            header << ";\n";
        }
    }
    if (!mod.functions.empty()) {
        header << '\n';
    }
}

void emit_module_source(std::ostringstream& source, const Source& src, const HirModule& mod) {
    const Source& here = mod.source ? *mod.source : src;
    const std::string qp_path = line_path(here.path());
    for (const auto& child : mod.mods) {
        source << "namespace " << child.name << " {\n\n";
        emit_module_source(source, here, child);
        source << "}  // namespace " << child.name << "\n\n";
    }

    emit_uses(source, mod);

    for (const auto& impl : mod.impls) {
        for (const auto& method : impl.methods) {
            if (method.is_extern || is_generic(method)) {
                continue;
            }
            const auto loc = here.location(method.offset);
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
        if (fn.is_extern || is_generic(fn)) {
            continue;
        }
        const auto loc = here.location(fn.offset);
        source << "#line " << loc.line << " \"" << qp_path << "\"\n";
        emit_free_signature(source, fn);
        emit_fn_body(source, fn);
    }
}

std::string emit_header(const HirModule& mod) {
    const auto methods = methods_by_type(mod);
    std::ostringstream header;
    header << "#pragma once\n\n";
    header << "#include <array>\n";
    header << "#include <cstdint>\n";
    header << "#include <cstdlib>\n";
    header << "#include <functional>\n";
    header << "#include <map>\n";
    header << "#include <string>\n";
    header << "#include <variant>\n";
    header << "#include <vector>\n\n";

    bool any_c_abi = false;
    for (const auto& fn : mod.functions) {
        if (!fn.c_abi) {
            continue;
        }
        if (!any_c_abi) {
            header << "extern \"C\" {\n";
            any_c_abi = true;
        }
        header << "    ";
        emit_free_signature(header, fn);
        header << ";\n";
    }
    if (any_c_abi) {
        header << "}\n\n";
    }

    header << "#if defined(__has_include)\n";
    header << "#  if __has_include(\"qplus_host.h\")\n";
    header << "#    include \"qplus_host.h\"\n";
    header << "#  endif\n";
    header << "#endif\n\n";

    header << "namespace qplus {\n\n";
    header << "using String = std::string;\n";
    header << "template <typename T>\nusing List = std::vector<T>;\n";
    header << "template <typename T, std::size_t N>\nusing Array = std::array<T, N>;\n";
    header << "template <typename K, typename V>\nusing Dict = std::map<K, V>;\n";
    header << "template <typename T>\nusing Fn = std::function<T>;\n\n";
    header << "template <typename T>\nT unwrap(T* p) {\n    if (p == nullptr) {\n        std::abort();\n    }\n    return *p;\n}\n\n";
    emit_module_header(header, mod, methods);
    header << "}  // namespace qplus\n";
    return header.str();
}

std::string emit_source(const Source& src, const HirModule& mod, std::string_view header_name) {
    std::ostringstream source;
    source << "#include \"" << header_name << "\"\n\n";
    source << "namespace qplus {\n\n";
    emit_module_source(source, src, mod);
    source << "}  // namespace qplus\n";
    return source.str();
}

}  // namespace

CppOutput emit_cpp(const Source& src, const HirModule& mod) {
    g_mod = &mod;
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

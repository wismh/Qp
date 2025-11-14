#include "compiler/lsp/analyze.hpp"

#include "compiler/frontend.hpp"
#include "compiler/lower.hpp"
#include "compiler/type.hpp"
#include "compiler/typeck.hpp"

#include <algorithm>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>

namespace qpc::lsp {
namespace {

[[nodiscard]] constexpr bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

[[nodiscard]] constexpr bool is_ident_continue(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

std::string type_expr_str(const TypeExpr& t) {
    switch (t.kind) {
        case TypeExpr::Kind::Named: {
            std::string out = t.name;
            if (!t.args.empty()) {
                out += '<';
                for (std::size_t i = 0; i < t.args.size(); ++i) {
                    if (i != 0) {
                        out += ", ";
                    }
                    out += type_expr_str(t.args[i]);
                }
                out += '>';
            }
            if (t.pack_expand) {
                out += "...";
            }
            return out;
        }
        case TypeExpr::Kind::Unit:
            return "()";
        case TypeExpr::Kind::List:
            return t.args.empty() ? "[]" : "[" + type_expr_str(t.args.front()) + "]";
        case TypeExpr::Kind::Array:
            return t.args.empty() ? "[; 0]"
                                  : "[" + type_expr_str(t.args.front()) + "; " +
                                        std::to_string(t.array_len) + "]";
        case TypeExpr::Kind::Dict:
            if (t.args.size() < 2) {
                return "{}";
            }
            return "{" + type_expr_str(t.args.front()) + ": " + type_expr_str(t.args.back()) + "}";
        case TypeExpr::Kind::Tuple: {
            std::string out = "(";
            for (std::size_t i = 0; i < t.args.size(); ++i) {
                if (i != 0) {
                    out += ", ";
                }
                out += type_expr_str(t.args[i]);
            }
            out += ")";
            return out;
        }
        case TypeExpr::Kind::Fn: {
            std::string out = "fn(";
            for (std::size_t i = 0; i + 1 < t.args.size(); ++i) {
                if (i != 0) {
                    out += ", ";
                }
                out += type_expr_str(t.args[i]);
            }
            out += ") -> ";
            out += t.args.empty() ? "()" : type_expr_str(t.args.back());
            return out;
        }
        case TypeExpr::Kind::Nullable:
            return t.args.empty() ? "?" : type_expr_str(t.args.front()) + "?";
        case TypeExpr::Kind::Dyn:
            return t.name.empty() ? "dyn" : "dyn " + t.name;
    }
    return "<type>";
}

std::string fn_sig(const std::string& name, const std::vector<HirParam>& params, const Type& ret) {
    std::string out = "fn " + name + "(";
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i != 0) {
            out += ", ";
        }
        out += type_name(params[i].ty);
    }
    out += ") -> " + type_name(ret);
    return out;
}

const char* const k_keywords[] = {
    "fn",     "let",    "mut",    "return", "pub",    "struct",   "impl",     "self",  "enum",
    "variant","match",  "for",    "true",   "false",  "extern",   "if",       "else",  "while",
    "in",     "break",  "continue","mod",   "use",    "from",     "trait",    "dyn",   "as",
    "null",   "new",    "ref",
};

const char* const k_primitives[] = {
    "bool", "char", "string", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64",
};

const char* const k_builtins[] = {
    "to_string", "type_id", "type_name", "field_count", "field_name", "field_type_name", "fn_name",
    "sin",       "cos",     "tan",       "asin",        "acos",       "atan",            "sqrt",
    "abs",       "floor",   "ceil",      "exp",         "ln",         "log2",            "atan2",
    "fmod",      "pow",
};

bool type_is_useful(const Type& ty) {
    return ty.kind != TypeKind::Unknown && ty.kind != TypeKind::Error;
}

void walk_pat(const HirPat* pat, auto&& fn);
void walk_expr(const HirExpr* expr, auto&& fn);
void walk_block(const HirBlock& block, auto&& fn);
void walk_stmt(const HirStmt* stmt, auto&& fn);

void walk_pat(const HirPat* pat, auto&& fn) {
    if (pat == nullptr) {
        return;
    }
    std::visit(
        [&](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, HirPatVariant>) {
                for (const auto& arg : k.args) {
                    walk_pat(arg.get(), fn);
                }
            }
        },
        pat->kind);
}

void walk_expr(const HirExpr* expr, auto&& fn) {
    if (expr == nullptr) {
        return;
    }
    fn(*expr);
    std::visit(
        [&](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, HirBinary>) {
                walk_expr(k.lhs.get(), fn);
                walk_expr(k.rhs.get(), fn);
            } else if constexpr (std::is_same_v<T, HirUnary>) {
                walk_expr(k.operand.get(), fn);
            } else if constexpr (std::is_same_v<T, HirCall>) {
                walk_expr(k.callee_expr.get(), fn);
                for (const auto& arg : k.args) {
                    walk_expr(arg.get(), fn);
                }
            } else if constexpr (std::is_same_v<T, HirAssign>) {
                walk_expr(k.value.get(), fn);
            } else if constexpr (std::is_same_v<T, HirFieldAccess>) {
                walk_expr(k.base.get(), fn);
            } else if constexpr (std::is_same_v<T, HirIndex>) {
                walk_expr(k.base.get(), fn);
                walk_expr(k.index.get(), fn);
            } else if constexpr (std::is_same_v<T, HirStructLit>) {
                for (const auto& f : k.fields) {
                    walk_expr(f.value.get(), fn);
                }
            } else if constexpr (std::is_same_v<T, HirEnumLit>) {
                for (const auto& f : k.fields) {
                    walk_expr(f.value.get(), fn);
                }
                for (const auto& arg : k.args) {
                    walk_expr(arg.get(), fn);
                }
            } else if constexpr (std::is_same_v<T, HirMethodCall>) {
                walk_expr(k.receiver.get(), fn);
                for (const auto& arg : k.args) {
                    walk_expr(arg.get(), fn);
                }
            } else if constexpr (std::is_same_v<T, HirFieldAssign>) {
                walk_expr(k.base.get(), fn);
                walk_expr(k.value.get(), fn);
            } else if constexpr (std::is_same_v<T, HirIndexAssign>) {
                walk_expr(k.base.get(), fn);
                walk_expr(k.index.get(), fn);
                walk_expr(k.value.get(), fn);
            } else if constexpr (std::is_same_v<T, HirMatch>) {
                walk_expr(k.scrutinee.get(), fn);
                for (const auto& arm : k.arms) {
                    walk_pat(arm.pat.get(), fn);
                    walk_expr(arm.body.get(), fn);
                }
            } else if constexpr (std::is_same_v<T, HirListLit>) {
                for (const auto& e : k.elems) {
                    walk_expr(e.get(), fn);
                }
            } else if constexpr (std::is_same_v<T, HirTupleLit>) {
                for (const auto& e : k.elems) {
                    walk_expr(e.get(), fn);
                }
            } else if constexpr (std::is_same_v<T, HirDictLit>) {
                for (const auto& [key, val] : k.entries) {
                    walk_expr(key.get(), fn);
                    walk_expr(val.get(), fn);
                }
            } else if constexpr (std::is_same_v<T, HirIf>) {
                walk_expr(k.cond.get(), fn);
                for (const auto& s : k.then_stmts) {
                    walk_stmt(s.get(), fn);
                }
                walk_expr(k.then_tail.get(), fn);
                walk_expr(k.else_expr.get(), fn);
            } else if constexpr (std::is_same_v<T, HirRange>) {
                walk_expr(k.start.get(), fn);
                walk_expr(k.end.get(), fn);
            } else if constexpr (std::is_same_v<T, HirClosure>) {
                walk_block(k.body, fn);
            } else if constexpr (std::is_same_v<T, HirCast> || std::is_same_v<T, HirUnwrap> ||
                                 std::is_same_v<T, HirTry>) {
                walk_expr(k.expr.get(), fn);
            } else if constexpr (std::is_same_v<T, HirNew>) {
                for (const auto& f : k.fields) {
                    walk_expr(f.value.get(), fn);
                }
            } else if constexpr (std::is_same_v<T, HirCoalesce>) {
                walk_expr(k.lhs.get(), fn);
                walk_expr(k.rhs.get(), fn);
            }
        },
        expr->kind);
}

void walk_stmt(const HirStmt* stmt, auto&& fn) {
    if (stmt == nullptr) {
        return;
    }
    std::visit(
        [&](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, HirLet>) {
                walk_expr(k.init.get(), fn);
            } else if constexpr (std::is_same_v<T, HirReturn>) {
                walk_expr(k.value.get(), fn);
            } else if constexpr (std::is_same_v<T, HirExprStmt>) {
                walk_expr(k.expr.get(), fn);
            } else if constexpr (std::is_same_v<T, HirWhile>) {
                walk_expr(k.cond.get(), fn);
                for (const auto& s : k.stmts) {
                    walk_stmt(s.get(), fn);
                }
                walk_expr(k.tail.get(), fn);
            } else if constexpr (std::is_same_v<T, HirFor>) {
                walk_expr(k.iter.get(), fn);
                for (const auto& s : k.stmts) {
                    walk_stmt(s.get(), fn);
                }
                walk_expr(k.tail.get(), fn);
            }
        },
        stmt->kind);
}

void walk_block(const HirBlock& block, auto&& fn) {
    for (const auto& s : block.stmts) {
        walk_stmt(s.get(), fn);
    }
    walk_expr(block.tail.get(), fn);
}

void walk_fn(const HirFn& fn, auto&& on_expr, auto&& on_fn) {
    on_fn(fn);
    walk_block(fn.body, on_expr);
}

void walk_module(const HirModule& mod, auto&& on_expr, auto&& on_fn) {
    for (const auto& st : mod.statics) {
        walk_expr(st.init.get(), on_expr);
    }
    for (const auto& impl : mod.impls) {
        for (const auto& method : impl.methods) {
            walk_fn(method, on_expr, on_fn);
        }
    }
    for (const auto& fn : mod.functions) {
        walk_fn(fn, on_expr, on_fn);
    }
    for (const auto& child : mod.mods) {
        walk_module(child, on_expr, on_fn);
    }
}

}  // namespace

LspDocument LspDocument::from_text(std::string path, std::string text) {
    LspDocument doc;
    doc.source_ = Source::from_string(std::move(path), std::move(text));
    doc.analyze();
    return doc;
}

void LspDocument::analyze() {
    ParseResult parsed = parse_with_mods(source_, extras_, diags_, false);
    tokens_ = std::move(parsed.tokens);
    if (!parsed.parsed) {
        return;
    }
    collect_file(parsed.ast);
    hir_ = lower(source_, std::move(parsed.ast), diags_);
    lowered_ = true;
    typeck(source_, hir_, diags_);
    typed_ = true;
}

void LspDocument::add_symbol(std::string name, CompletionKind kind, std::string detail, std::size_t offset,
                             std::size_t fn_offset, bool local) {
    if (name.empty()) {
        return;
    }
    symbols_.push_back(Symbol{
        .name = std::move(name),
        .kind = kind,
        .detail = std::move(detail),
        .offset = offset,
        .fn_offset = fn_offset,
        .local = local,
    });
}

void LspDocument::collect_file(const AstFile& file) {
    for (const auto& st : file.statics) {
        add_symbol(st.name, CompletionKind::Variable, "let", st.offset, 0, false);
    }
    for (const auto& tr : file.traits) {
        add_symbol(tr.name, CompletionKind::Type, "trait", tr.offset, 0, false);
        for (const auto& m : tr.methods) {
            add_symbol(m.name, CompletionKind::Function, "fn", m.offset, 0, false);
        }
    }
    for (const auto& st : file.structs) {
        add_symbol(st.name, CompletionKind::Type, "struct", st.offset, 0, false);
    }
    for (const auto& en : file.enums) {
        add_symbol(en.name, CompletionKind::Type, "enum", en.offset, 0, false);
        for (const auto& mem : en.members) {
            add_symbol(mem.name, CompletionKind::Constant, "enum", mem.offset, 0, false);
        }
    }
    for (const auto& vd : file.variants) {
        add_symbol(vd.name, CompletionKind::Type, "variant", vd.offset, 0, false);
        for (const auto& v : vd.variants) {
            add_symbol(v.name, CompletionKind::Constant, "variant", v.offset, 0, false);
        }
    }
    for (const auto& impl : file.impls) {
        for (const auto& method : impl.methods) {
            collect_fn(method);
        }
    }
    for (const auto& fn : file.functions) {
        collect_fn(fn);
    }
    for (const auto& m : file.mods) {
        if (m.body) {
            collect_file(*m.body);
        }
    }
}

void LspDocument::collect_fn(const FnDecl& fn) {
    add_symbol(fn.name, CompletionKind::Function, "fn", fn.offset, 0, false);
    for (const auto& p : fn.params) {
        add_symbol(p.name, CompletionKind::Variable, "param " + type_expr_str(p.ty), p.offset, fn.offset,
                   true);
    }
    collect_block(fn.body, fn.offset);
}

void LspDocument::collect_block(const Block& block, std::size_t fn_offset) {
    for (const auto& stmt : block.stmts) {
        if (stmt) {
            collect_stmt(*stmt, fn_offset);
        }
    }
    collect_expr(block.tail.get(), fn_offset);
}

void LspDocument::collect_stmt(const Stmt& stmt, std::size_t fn_offset) {
    std::visit(
        [&](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, StmtLet>) {
                std::string detail = "let";
                if (k.ty) {
                    detail += " " + type_expr_str(*k.ty);
                }
                add_symbol(k.name, CompletionKind::Variable, std::move(detail), stmt.offset, fn_offset, true);
                collect_expr(k.init.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, StmtReturn>) {
                collect_expr(k.value.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, StmtExpr>) {
                collect_expr(k.expr.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, StmtWhile>) {
                collect_expr(k.cond.get(), fn_offset);
                if (k.body) {
                    collect_block(*k.body, fn_offset);
                }
            } else if constexpr (std::is_same_v<T, StmtFor>) {
                add_symbol(k.name, CompletionKind::Variable, "let", stmt.offset, fn_offset, true);
                if (!k.second.empty()) {
                    add_symbol(k.second, CompletionKind::Variable, "let", stmt.offset, fn_offset, true);
                }
                collect_expr(k.iter.get(), fn_offset);
                if (k.body) {
                    collect_block(*k.body, fn_offset);
                }
            }
        },
        stmt.kind);
}

void LspDocument::collect_expr(const Expr* expr, std::size_t fn_offset) {
    if (expr == nullptr) {
        return;
    }
    std::visit(
        [&](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, ExprBinary>) {
                collect_expr(k.lhs.get(), fn_offset);
                collect_expr(k.rhs.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, ExprUnary>) {
                collect_expr(k.operand.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, ExprCall>) {
                collect_expr(k.callee.get(), fn_offset);
                for (const auto& arg : k.args) {
                    collect_expr(arg.get(), fn_offset);
                }
            } else if constexpr (std::is_same_v<T, ExprAssign>) {
                collect_expr(k.lhs.get(), fn_offset);
                collect_expr(k.rhs.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, ExprField>) {
                collect_expr(k.base.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, ExprIndex>) {
                collect_expr(k.base.get(), fn_offset);
                collect_expr(k.index.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, ExprStructLit>) {
                for (const auto& f : k.fields) {
                    collect_expr(f.value.get(), fn_offset);
                }
            } else if constexpr (std::is_same_v<T, ExprMatch>) {
                collect_expr(k.scrutinee.get(), fn_offset);
                for (const auto& arm : k.arms) {
                    collect_expr(arm.body.get(), fn_offset);
                }
            } else if constexpr (std::is_same_v<T, ExprListLit> || std::is_same_v<T, ExprTuple>) {
                for (const auto& e : k.elems) {
                    collect_expr(e.get(), fn_offset);
                }
            } else if constexpr (std::is_same_v<T, ExprDictLit>) {
                for (const auto& e : k.entries) {
                    collect_expr(e.key.get(), fn_offset);
                    collect_expr(e.value.get(), fn_offset);
                }
            } else if constexpr (std::is_same_v<T, ExprIf>) {
                collect_expr(k.cond.get(), fn_offset);
                if (!k.let_name.empty()) {
                    add_symbol(k.let_name, CompletionKind::Variable, "let", expr->offset, fn_offset, true);
                }
                if (k.then_block) {
                    collect_block(*k.then_block, fn_offset);
                }
                collect_expr(k.else_expr.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, ExprRange>) {
                collect_expr(k.start.get(), fn_offset);
                collect_expr(k.end.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, ExprClosure>) {
                for (const auto& p : k.params) {
                    add_symbol(p.name, CompletionKind::Variable, "param " + type_expr_str(p.ty), p.offset,
                               fn_offset, true);
                }
                if (k.body) {
                    collect_block(*k.body, fn_offset);
                }
            } else if constexpr (std::is_same_v<T, ExprCast>) {
                collect_expr(k.expr.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, ExprUnwrap> || std::is_same_v<T, ExprTry>) {
                collect_expr(k.expr.get(), fn_offset);
            } else if constexpr (std::is_same_v<T, ExprNew>) {
                for (const auto& f : k.fields) {
                    collect_expr(f.value.get(), fn_offset);
                }
            } else if constexpr (std::is_same_v<T, ExprCoalesce>) {
                collect_expr(k.lhs.get(), fn_offset);
                collect_expr(k.rhs.get(), fn_offset);
            }
        },
        expr->kind);
}

LspPos LspDocument::pos_from_offset(std::size_t offset) const {
    const Loc loc = source_.location(offset);
    LspPos pos;
    pos.line = loc.line == 0 ? 0 : loc.line - 1;
    pos.character = loc.column == 0 ? 0 : loc.column - 1;
    return pos;
}

std::size_t LspDocument::offset_from_pos(LspPos pos) const {
    const std::string& text = source_.text();
    std::size_t i = 0;
    std::uint32_t line = 0;
    while (i < text.size() && line < pos.line) {
        if (text[i] == '\n') {
            ++line;
        }
        ++i;
    }
    std::size_t col = 0;
    while (i < text.size() && text[i] != '\n' && col < pos.character) {
        ++i;
        ++col;
    }
    return i;
}

std::string LspDocument::ident_prefix(std::size_t offset) const {
    const std::string& text = source_.text();
    if (offset > text.size()) {
        offset = text.size();
    }
    std::size_t start = offset;
    while (start > 0 && is_ident_continue(text[start - 1])) {
        --start;
    }
    if (start == offset || !is_ident_start(text[start])) {
        return {};
    }
    return text.substr(start, offset - start);
}

const Token* LspDocument::token_at(std::size_t offset) const {
    const Token* best = nullptr;
    for (const auto& t : tokens_) {
        if (t.kind == TokenKind::Eof || t.length == 0) {
            continue;
        }
        if (offset >= t.offset && offset <= t.offset + t.length) {
            best = &t;
            if (offset < t.offset + t.length) {
                break;
            }
        }
    }
    return best;
}

std::size_t LspDocument::enclosing_fn_offset(std::size_t offset) const {
    std::size_t best = 0;
    bool found = false;
    for (const auto& sym : symbols_) {
        if (sym.kind != CompletionKind::Function || sym.local) {
            continue;
        }
        if (sym.offset <= offset && (!found || sym.offset >= best)) {
            best = sym.offset;
            found = true;
        }
    }
    return found ? best : 0;
}

std::vector<CompletionItem> LspDocument::completions(std::size_t offset) const {
    const std::string prefix = ident_prefix(offset);
    std::vector<CompletionItem> items;
    auto add = [&](std::string label, CompletionKind kind, std::string detail) {
        if (!prefix.empty() && !label.starts_with(prefix)) {
            return;
        }
        items.push_back(CompletionItem{
            .label = std::move(label),
            .kind = kind,
            .detail = std::move(detail),
        });
    };

    for (const char* kw : k_keywords) {
        add(kw, CompletionKind::Keyword, "keyword");
    }
    for (const char* ty : k_primitives) {
        add(ty, CompletionKind::Type, "type");
    }
    for (const char* b : k_builtins) {
        add(b, CompletionKind::Function, "builtin");
    }

    const std::size_t fn_off = enclosing_fn_offset(offset);
    for (const auto& sym : symbols_) {
        if (sym.local) {
            if (sym.fn_offset != fn_off || sym.offset > offset) {
                continue;
            }
        }
        add(sym.name, sym.kind, sym.detail);
    }

    std::unordered_set<std::string> seen;
    std::vector<CompletionItem> unique;
    unique.reserve(items.size());
    for (auto& item : items) {
        if (!seen.insert(item.label).second) {
            continue;
        }
        unique.push_back(std::move(item));
    }
    return unique;
}

std::optional<Hover> LspDocument::hover_from_hir(std::size_t ident_offset, std::string_view name) const {
    if (!lowered_) {
        return std::nullopt;
    }

    std::optional<Hover> found;
    auto on_expr = [&](const HirExpr& e) {
        if (found) {
            return;
        }
        if (e.offset != ident_offset) {
            return;
        }
        if (const auto* var = std::get_if<HirVar>(&e.kind)) {
            if (var->name != name) {
                return;
            }
            if (typed_ && type_is_useful(e.ty)) {
                found = Hover{.contents = type_name(e.ty)};
                return;
            }
            found = Hover{.contents = var->fn_value ? "fn" : "let"};
        }
    };
    auto on_fn = [&](const HirFn& fn) {
        if (found) {
            return;
        }
        if (fn.offset == ident_offset && fn.name == name) {
            found = Hover{.contents = fn_sig(fn.name, fn.params, fn.return_ty)};
            return;
        }
        for (const auto& p : fn.params) {
            if (p.offset == ident_offset && p.name == name) {
                if (type_is_useful(p.ty)) {
                    found = Hover{.contents = type_name(p.ty)};
                } else {
                    found = Hover{.contents = "param"};
                }
                return;
            }
        }
    };
    walk_module(hir_, on_expr, on_fn);

    if (!found) {
        auto check_named = [&](std::size_t offset, std::string_view n, std::string detail) {
            if (!found && offset == ident_offset && n == name) {
                found = Hover{.contents = std::move(detail)};
            }
        };
        auto walk_mod = [&](auto&& self, const HirModule& mod) -> void {
            for (const auto& st : mod.structs) {
                check_named(st.offset, st.name, "struct " + st.name);
            }
            for (const auto& en : mod.enums) {
                check_named(en.offset, en.name, "enum " + en.name);
            }
            for (const auto& vd : mod.variants) {
                check_named(vd.offset, vd.name, "variant " + vd.name);
            }
            for (const auto& tr : mod.traits) {
                check_named(tr.offset, tr.name, "trait " + tr.name);
            }
            for (const auto& st : mod.statics) {
                if (st.offset == ident_offset && st.name == name) {
                    found = Hover{.contents = type_is_useful(st.ty) ? type_name(st.ty) : "let"};
                }
            }
            for (const auto& child : mod.mods) {
                self(self, child);
            }
        };
        walk_mod(walk_mod, hir_);
    }
    return found;
}

std::optional<Hover> LspDocument::hover_from_symbols(std::size_t ident_offset, std::string_view name) const {
    const LspDocument::Symbol* by_offset = nullptr;
    const LspDocument::Symbol* by_name = nullptr;
    for (const auto& sym : symbols_) {
        if (sym.name != name) {
            continue;
        }
        if (sym.offset == ident_offset) {
            by_offset = &sym;
            break;
        }
        if (by_name == nullptr) {
            by_name = &sym;
        }
    }
    const auto* sym = by_offset != nullptr ? by_offset : by_name;
    if (sym == nullptr) {
        return std::nullopt;
    }
    if (!sym->detail.empty()) {
        return Hover{.contents = sym->detail};
    }
    return Hover{.contents = std::string(name)};
}

std::optional<Hover> LspDocument::hover(std::size_t offset) const {
    const Token* tok = token_at(offset);
    if (tok == nullptr) {
        return std::nullopt;
    }
    const bool identish = tok->kind == TokenKind::Ident || tok->kind == TokenKind::KwSelf;
    if (!identish) {
        return std::nullopt;
    }
    const std::string name(tok->text(source_.view()));
    if (auto h = hover_from_hir(tok->offset, name)) {
        return h;
    }
    return hover_from_symbols(tok->offset, name);
}

}  // namespace qpc::lsp

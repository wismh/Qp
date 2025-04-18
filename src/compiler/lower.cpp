#include "compiler/lower.hpp"

#include "compiler/token.hpp"
#include "compiler/type.hpp"

#include <charconv>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace qpc {
namespace {

std::string strip_underscores(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (c != '_') {
            out.push_back(c);
        }
    }
    return out;
}

template <class T>
T parse_number(std::string_view raw, const Source& src, std::size_t offset, DiagnosticEngine& diags,
               const char* invalid_message) {
    const std::string cleaned = strip_underscores(raw);
    T value{};
    const auto* begin = cleaned.data();
    const auto* end = begin + cleaned.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end) {
        diags.error(src, offset, invalid_message);
        return T{};
    }
    return value;
}

std::int32_t parse_i32(std::string_view raw, const Source& src, std::size_t offset,
                       DiagnosticEngine& diags) {
    return parse_number<std::int32_t>(raw, src, offset, diags, "invalid i32 literal");
}

float parse_f32(std::string_view raw, const Source& src, std::size_t offset, DiagnosticEngine& diags) {
    return parse_number<float>(raw, src, offset, diags, "invalid f32 literal");
}

BinOp binop_from_token(TokenKind kind) {
    switch (kind) {
        case TokenKind::Plus:
            return BinOp::Add;
        case TokenKind::Minus:
            return BinOp::Sub;
        case TokenKind::Star:
            return BinOp::Mul;
        case TokenKind::Slash:
            return BinOp::Div;
        case TokenKind::Percent:
            return BinOp::Mod;
        default:
            return BinOp::Add;
    }
}

HirExprPtr lower_expr(const Source& src, ExprPtr expr, DiagnosticEngine& diags);

std::string callee_name(const Source& src, const Expr& callee, DiagnosticEngine& diags) {
    if (const auto* ident = std::get_if<ExprIdent>(&callee.kind)) {
        return ident->name;
    }
    diags.error(src, callee.offset, "callee must be a function name");
    return "<error>";
}

std::string assign_target(const Source& src, const Expr& lhs, DiagnosticEngine& diags) {
    if (const auto* ident = std::get_if<ExprIdent>(&lhs.kind)) {
        return ident->name;
    }
    diags.error(src, lhs.offset, "assignment target must be a variable");
    return "<error>";
}

HirStmtPtr lower_stmt(const Source& src, StmtPtr stmt, DiagnosticEngine& diags) {
    auto out = std::make_unique<HirStmt>();
    out->offset = stmt->offset;

    std::visit(
        [&](auto&& kind) {
            using K = std::decay_t<decltype(kind)>;
            if constexpr (std::is_same_v<K, StmtLet>) {
                HirLet let;
                let.mut = kind.mut;
                let.name = std::move(kind.name);
                if (kind.ty) {
                    let.ty = type_from_name(*kind.ty);
                    if (let.ty == Type::error()) {
                        diags.error(src, stmt->offset, "unknown type '" + *kind.ty + "'");
                    }
                }
                let.init = lower_expr(src, std::move(kind.init), diags);
                out->kind = std::move(let);
            } else if constexpr (std::is_same_v<K, StmtReturn>) {
                HirReturn ret;
                if (kind.value) {
                    ret.value = lower_expr(src, std::move(kind.value), diags);
                }
                out->kind = std::move(ret);
            } else if constexpr (std::is_same_v<K, StmtExpr>) {
                out->kind = HirExprStmt{lower_expr(src, std::move(kind.expr), diags)};
            }
        },
        stmt->kind);

    return out;
}

HirBlock lower_block(const Source& src, Block block, DiagnosticEngine& diags) {
    HirBlock out;
    out.offset = block.offset;
    out.stmts.reserve(block.stmts.size());

    for (auto& stmt : block.stmts) {
        out.stmts.push_back(lower_stmt(src, std::move(stmt), diags));
    }
    if (block.tail) {
        out.tail = lower_expr(src, std::move(block.tail), diags);
    }
    return out;
}

HirExprPtr lower_expr(const Source& src, ExprPtr expr, DiagnosticEngine& diags) {
    auto out = std::make_unique<HirExpr>();
    out->offset = expr->offset;

    std::visit(
        [&](auto&& kind) {
            using K = std::decay_t<decltype(kind)>;
            if constexpr (std::is_same_v<K, LitInt>) {
                out->ty = Type::i32();
                out->kind = HirLitInt{parse_i32(kind.raw, src, expr->offset, diags)};
            } else if constexpr (std::is_same_v<K, LitFloat>) {
                out->ty = Type::f32();
                out->kind = HirLitFloat{parse_f32(kind.raw, src, expr->offset, diags)};
            } else if constexpr (std::is_same_v<K, ExprIdent>) {
                out->kind = HirVar{std::move(kind.name)};
            } else if constexpr (std::is_same_v<K, ExprBinary>) {
                out->kind = HirBinary{
                    binop_from_token(kind.op),
                    lower_expr(src, std::move(kind.lhs), diags),
                    lower_expr(src, std::move(kind.rhs), diags),
                };
            } else if constexpr (std::is_same_v<K, ExprUnary>) {
                out->kind = HirUnary{UnOp::Neg, lower_expr(src, std::move(kind.operand), diags)};
            } else if constexpr (std::is_same_v<K, ExprCall>) {
                HirCall call;
                call.callee = callee_name(src, *kind.callee, diags);
                call.args.reserve(kind.args.size());
                for (auto& arg : kind.args) {
                    call.args.push_back(lower_expr(src, std::move(arg), diags));
                }
                out->kind = std::move(call);
            } else if constexpr (std::is_same_v<K, ExprAssign>) {
                out->kind = HirAssign{
                    assign_target(src, *kind.lhs, diags),
                    lower_expr(src, std::move(kind.rhs), diags),
                };
            }
        },
        expr->kind);

    return out;
}

Type parse_return_ty(const Source& src, const FnDecl& fn, DiagnosticEngine& diags) {
    if (!fn.return_ty) {
        return Type::unit();
    }

    const Type ty = type_from_name(*fn.return_ty);
    if (ty == Type::error()) {
        diags.error(src, fn.offset, "unknown return type '" + *fn.return_ty + "'");
        return Type::error();
    }
    return ty;
}

HirFn lower_fn(const Source& src, FnDecl& fn, DiagnosticEngine& diags) {
    HirFn hfn;
    hfn.pub = fn.pub;
    hfn.name = std::move(fn.name);
    hfn.offset = fn.offset;
    hfn.return_ty = parse_return_ty(src, fn, diags);
    hfn.params.reserve(fn.params.size());

    for (auto& p : fn.params) {
        HirParam hp;
        hp.name = std::move(p.name);
        hp.offset = p.offset;
        hp.ty = type_from_name(p.ty);
        if (hp.ty == Type::error()) {
            diags.error(src, p.offset, "unknown type '" + p.ty + "'");
        }
        hfn.params.push_back(std::move(hp));
    }

    hfn.body = lower_block(src, std::move(fn.body), diags);
    return hfn;
}

}  // namespace

HirModule lower(const Source& src, AstFile ast, DiagnosticEngine& diags) {
    HirModule mod;
    mod.functions.reserve(ast.functions.size());

    for (auto& fn : ast.functions) {
        mod.functions.push_back(lower_fn(src, fn, diags));
    }
    return mod;
}

}  // namespace qpc

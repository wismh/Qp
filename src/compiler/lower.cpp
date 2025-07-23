#include "compiler/lower.hpp"

#include "compiler/token.hpp"
#include "compiler/type.hpp"

#include "compiler/ast.hpp"

#include <charconv>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace qpc {
namespace {

Type lower_type(const TypeExpr& te) {
    switch (te.kind) {
        case TypeExpr::Kind::Unit:
            return Type::unit();
        case TypeExpr::Kind::Named:
            return type_from_name(te.name);
        case TypeExpr::Kind::List:
            return Type::list(lower_type(te.args.front()));
        case TypeExpr::Kind::Array:
            return Type::array(lower_type(te.args.front()), te.array_len);
        case TypeExpr::Kind::Dict:
            return Type::dict(lower_type(te.args.front()), lower_type(te.args.back()));
        case TypeExpr::Kind::Fn: {
            if (te.args.empty()) {
                return Type::error();
            }
            std::vector<Type> params;
            for (std::size_t i = 0; i + 1 < te.args.size(); ++i) {
                params.push_back(lower_type(te.args[i]));
            }
            return Type::fn(std::move(params), lower_type(te.args.back()));
        }
        case TypeExpr::Kind::Nullable:
            return Type::nullable(lower_type(te.args.front()));
    }
    return Type::error();
}

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

std::int64_t parse_i64(std::string_view raw, const Source& src, std::size_t offset,
                       DiagnosticEngine& diags) {
    return parse_number<std::int64_t>(raw, src, offset, diags, "invalid integer literal");
}

double parse_f64(std::string_view raw, const Source& src, std::size_t offset, DiagnosticEngine& diags) {
    return parse_number<double>(raw, src, offset, diags, "invalid float literal");
}

char32_t unescape_char(std::string_view raw, const Source& src, std::size_t offset,
                       DiagnosticEngine& diags) {
    if (raw.size() < 3 || raw.front() != '\'' || raw.back() != '\'') {
        diags.error(src, offset, "invalid char literal");
        return 0;
    }
    if (raw[1] == '\\') {
        if (raw.size() < 4) {
            diags.error(src, offset, "invalid char literal");
            return 0;
        }
        switch (raw[2]) {
            case 'n':
                return U'\n';
            case 't':
                return U'\t';
            case 'r':
                return U'\r';
            case '\\':
                return U'\\';
            case '\'':
                return U'\'';
            case '0':
                return U'\0';
            default:
                diags.error(src, offset, "unknown char escape");
                return 0;
        }
    }
    return static_cast<unsigned char>(raw[1]);
}

std::string unescape_string(std::string_view raw, const Source& src, std::size_t offset,
                            DiagnosticEngine& diags) {
    if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') {
        diags.error(src, offset, "invalid string literal");
        return {};
    }
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 1; i + 1 < raw.size(); ++i) {
        if (raw[i] != '\\') {
            out.push_back(raw[i]);
            continue;
        }
        ++i;
        if (i + 1 >= raw.size()) {
            diags.error(src, offset, "unterminated string escape");
            break;
        }
        switch (raw[i]) {
            case 'n':
                out.push_back('\n');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '"':
                out.push_back('"');
                break;
            case '0':
                out.push_back('\0');
                break;
            default:
                diags.error(src, offset, "unknown string escape");
                break;
        }
    }
    return out;
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
        case TokenKind::EqEq:
            return BinOp::Eq;
        case TokenKind::BangEq:
            return BinOp::Ne;
        case TokenKind::Lt:
            return BinOp::Lt;
        case TokenKind::Le:
            return BinOp::Le;
        case TokenKind::Gt:
            return BinOp::Gt;
        case TokenKind::Ge:
            return BinOp::Ge;
        case TokenKind::AmpAmp:
            return BinOp::And;
        case TokenKind::PipePipe:
            return BinOp::Or;
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

SelfKind self_kind_from(SelfParam param) {
    switch (param) {
        case SelfParam::Value:
            return SelfKind::Value;
        case SelfParam::Mut:
            return SelfKind::Mut;
        case SelfParam::None:
            return SelfKind::None;
    }
    return SelfKind::None;
}

HirPatPtr lower_pat(const Source& src, PatPtr pat, DiagnosticEngine& diags) {
    auto out = std::make_unique<HirPat>();
    out->offset = pat->offset;
    std::visit(
        [&](auto&& kind) {
            using K = std::decay_t<decltype(kind)>;
            if constexpr (std::is_same_v<K, PatWild>) {
                out->kind = HirPatWild{};
            } else if constexpr (std::is_same_v<K, PatIdent>) {
                out->kind = HirPatBinding{std::move(kind.name)};
            } else if constexpr (std::is_same_v<K, PatVariant>) {
                HirPatVariant v;
                if (kind.path.size() == 1) {
                    v.variant = kind.path[0];
                } else if (kind.path.size() >= 2) {
                    v.enum_name = kind.path[0];
                    v.variant = kind.path[1];
                }
                v.tuple = kind.tuple;
                v.fields = std::move(kind.fields);
                v.args.reserve(kind.args.size());
                for (auto& arg : kind.args) {
                    v.args.push_back(lower_pat(src, std::move(arg), diags));
                }
                out->kind = std::move(v);
            }
        },
        pat->kind);
    return out;
}

HirExprPtr lower_expr(const Source& src, ExprPtr expr, DiagnosticEngine& diags);
HirBlock lower_block(const Source& src, Block block, DiagnosticEngine& diags);

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
                    let.ty = lower_type(*kind.ty);
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
            } else if constexpr (std::is_same_v<K, StmtWhile>) {
                HirWhile w;
                w.cond = lower_expr(src, std::move(kind.cond), diags);
                auto body = lower_block(src, std::move(*kind.body), diags);
                w.stmts = std::move(body.stmts);
                w.tail = std::move(body.tail);
                out->kind = std::move(w);
            } else if constexpr (std::is_same_v<K, StmtFor>) {
                HirFor f;
                f.name = std::move(kind.name);
                f.iter = lower_expr(src, std::move(kind.iter), diags);
                auto body = lower_block(src, std::move(*kind.body), diags);
                f.stmts = std::move(body.stmts);
                f.tail = std::move(body.tail);
                out->kind = std::move(f);
            } else if constexpr (std::is_same_v<K, StmtBreak>) {
                out->kind = HirBreak{};
            } else if constexpr (std::is_same_v<K, StmtContinue>) {
                out->kind = HirContinue{};
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
                HirLitInt lit;
                lit.value = parse_i64(kind.raw, src, expr->offset, diags);
                if (kind.suffix) {
                    lit.ty = type_from_suffix(*kind.suffix);
                    lit.unsuffixed = false;
                    if (!is_int(lit.ty)) {
                        diags.error(src, expr->offset, "integer suffix must be an integer type");
                        lit.ty = Type::error();
                    }
                }
                out->ty = lit.ty;
                out->kind = std::move(lit);
            } else if constexpr (std::is_same_v<K, LitFloat>) {
                HirLitFloat lit;
                lit.value = parse_f64(kind.raw, src, expr->offset, diags);
                if (kind.suffix) {
                    lit.ty = type_from_suffix(*kind.suffix);
                    lit.unsuffixed = false;
                    if (!is_float(lit.ty)) {
                        diags.error(src, expr->offset, "float suffix must be f32 or f64");
                        lit.ty = Type::error();
                    }
                }
                out->ty = lit.ty;
                out->kind = std::move(lit);
            } else if constexpr (std::is_same_v<K, LitBool>) {
                out->ty = Type::boolean();
                out->kind = HirLitBool{kind.value};
            } else if constexpr (std::is_same_v<K, LitChar>) {
                out->ty = Type::char_();
                out->kind = HirLitChar{unescape_char(kind.raw, src, expr->offset, diags)};
            } else if constexpr (std::is_same_v<K, LitString>) {
                out->ty = Type::string();
                out->kind = HirLitString{unescape_string(kind.raw, src, expr->offset, diags)};
            } else if constexpr (std::is_same_v<K, LitNull>) {
                out->kind = HirLitNull{};
            } else if constexpr (std::is_same_v<K, ExprIdent>) {
                out->kind = HirVar{std::move(kind.name)};
            } else if constexpr (std::is_same_v<K, ExprPath>) {
                if (kind.segments.size() == 2) {
                    HirEnumLit lit;
                    lit.enum_name = kind.segments[0];
                    lit.variant = kind.segments[1];
                    out->kind = std::move(lit);
                } else {
                    diags.error(src, expr->offset, "expected Type::Variant");
                    out->kind = HirVar{"<error>"};
                }
            } else if constexpr (std::is_same_v<K, ExprBinary>) {
                out->kind = HirBinary{
                    binop_from_token(kind.op),
                    lower_expr(src, std::move(kind.lhs), diags),
                    lower_expr(src, std::move(kind.rhs), diags),
                };
            } else if constexpr (std::is_same_v<K, ExprUnary>) {
                out->kind = HirUnary{
                    kind.op == TokenKind::Bang ? UnOp::Not : UnOp::Neg,
                    lower_expr(src, std::move(kind.operand), diags),
                };
            } else if constexpr (std::is_same_v<K, ExprCall>) {
                if (auto* field = std::get_if<ExprField>(&kind.callee->kind)) {
                    std::string method = std::move(field->name);
                    const bool null_safe = field->null_safe;
                    HirExprPtr receiver = lower_expr(src, std::move(field->base), diags);
                    HirMethodCall call;
                    call.receiver = std::move(receiver);
                    call.method = std::move(method);
                    call.null_safe = null_safe;
                    call.type_args.reserve(kind.type_args.size());
                    for (auto& ta : kind.type_args) {
                        call.type_args.push_back(lower_type(ta));
                    }
                    call.args.reserve(kind.args.size());
                    for (auto& arg : kind.args) {
                        call.args.push_back(lower_expr(src, std::move(arg), diags));
                    }
                    out->kind = std::move(call);
                } else if (auto* path = std::get_if<ExprPath>(&kind.callee->kind);
                           path && path->segments.size() == 2) {
                    HirEnumLit lit;
                    lit.enum_name = path->segments[0];
                    lit.variant = path->segments[1];
                    lit.tuple = true;
                    lit.args.reserve(kind.args.size());
                    for (auto& arg : kind.args) {
                        lit.args.push_back(lower_expr(src, std::move(arg), diags));
                    }
                    out->kind = std::move(lit);
                } else {
                    HirCall call;
                    if (std::holds_alternative<ExprIdent>(kind.callee->kind)) {
                        call.callee = callee_name(src, *kind.callee, diags);
                    } else {
                        call.callee_expr = lower_expr(src, std::move(kind.callee), diags);
                    }
                    call.type_args.reserve(kind.type_args.size());
                    for (auto& ta : kind.type_args) {
                        call.type_args.push_back(lower_type(ta));
                    }
                    call.args.reserve(kind.args.size());
                    for (auto& arg : kind.args) {
                        call.args.push_back(lower_expr(src, std::move(arg), diags));
                    }
                    out->kind = std::move(call);
                }
            } else if constexpr (std::is_same_v<K, ExprAssign>) {
                if (auto* field = std::get_if<ExprField>(&kind.lhs->kind)) {
                    std::string name = std::move(field->name);
                    HirExprPtr base = lower_expr(src, std::move(field->base), diags);
                    HirExprPtr value = lower_expr(src, std::move(kind.rhs), diags);
                    out->kind = HirFieldAssign{std::move(base), std::move(name), std::move(value)};
                } else if (auto* index = std::get_if<ExprIndex>(&kind.lhs->kind)) {
                    HirExprPtr base = lower_expr(src, std::move(index->base), diags);
                    HirExprPtr idx = lower_expr(src, std::move(index->index), diags);
                    HirExprPtr value = lower_expr(src, std::move(kind.rhs), diags);
                    out->kind = HirIndexAssign{std::move(base), std::move(idx), std::move(value)};
                } else {
                    out->kind = HirAssign{
                        assign_target(src, *kind.lhs, diags),
                        lower_expr(src, std::move(kind.rhs), diags),
                    };
                }
            } else if constexpr (std::is_same_v<K, ExprField>) {
                out->kind = HirFieldAccess{
                    lower_expr(src, std::move(kind.base), diags),
                    std::move(kind.name),
                    kind.null_safe,
                };
            } else if constexpr (std::is_same_v<K, ExprIndex>) {
                out->kind = HirIndex{
                    lower_expr(src, std::move(kind.base), diags),
                    lower_expr(src, std::move(kind.index), diags),
                };
            } else if constexpr (std::is_same_v<K, ExprStructLit>) {
                if (kind.path.size() >= 2) {
                    HirEnumLit lit;
                    lit.enum_name = kind.path[0];
                    lit.variant = kind.path[1];
                    lit.fields.reserve(kind.fields.size());
                    for (auto& field : kind.fields) {
                        lit.fields.push_back(HirStructLitField{
                            std::move(field.name),
                            lower_expr(src, std::move(field.value), diags),
                        });
                    }
                    out->kind = std::move(lit);
                } else {
                    HirStructLit lit;
                    lit.name = kind.name.empty() && !kind.path.empty() ? kind.path[0] : std::move(kind.name);
                    lit.fields.reserve(kind.fields.size());
                    for (auto& field : kind.fields) {
                        lit.fields.push_back(HirStructLitField{
                            std::move(field.name),
                            lower_expr(src, std::move(field.value), diags),
                        });
                    }
                    out->kind = std::move(lit);
                }
            } else if constexpr (std::is_same_v<K, ExprMatch>) {
                HirMatch match;
                match.scrutinee = lower_expr(src, std::move(kind.scrutinee), diags);
                match.arms.reserve(kind.arms.size());
                for (auto& arm : kind.arms) {
                    HirMatchArm hir_arm;
                    hir_arm.pat = lower_pat(src, std::move(arm.pat), diags);
                    hir_arm.body = lower_expr(src, std::move(arm.body), diags);
                    match.arms.push_back(std::move(hir_arm));
                }
                out->kind = std::move(match);
            } else if constexpr (std::is_same_v<K, ExprListLit>) {
                HirListLit lit;
                lit.elems.reserve(kind.elems.size());
                for (auto& elem : kind.elems) {
                    lit.elems.push_back(lower_expr(src, std::move(elem), diags));
                }
                out->kind = std::move(lit);
            } else if constexpr (std::is_same_v<K, ExprDictLit>) {
                HirDictLit lit;
                lit.entries.reserve(kind.entries.size());
                for (auto& entry : kind.entries) {
                    lit.entries.emplace_back(lower_expr(src, std::move(entry.key), diags),
                                             lower_expr(src, std::move(entry.value), diags));
                }
                out->kind = std::move(lit);
            } else if constexpr (std::is_same_v<K, ExprIf>) {
                HirIf hi;
                hi.cond = lower_expr(src, std::move(kind.cond), diags);
                auto then_b = lower_block(src, std::move(*kind.then_block), diags);
                hi.then_stmts = std::move(then_b.stmts);
                hi.then_tail = std::move(then_b.tail);
                if (kind.else_expr) {
                    hi.else_expr = lower_expr(src, std::move(kind.else_expr), diags);
                }
                out->kind = std::move(hi);
            } else if constexpr (std::is_same_v<K, ExprRange>) {
                out->kind = HirRange{
                    lower_expr(src, std::move(kind.start), diags),
                    lower_expr(src, std::move(kind.end), diags),
                };
            } else if constexpr (std::is_same_v<K, ExprClosure>) {
                HirClosure clo;
                clo.params.reserve(kind.params.size());
                for (auto& p : kind.params) {
                    HirParam hp;
                    hp.name = std::move(p.name);
                    hp.offset = p.offset;
                    hp.ty = lower_type(p.ty);
                    clo.params.push_back(std::move(hp));
                }
                if (kind.return_ty) {
                    clo.return_ty = lower_type(*kind.return_ty);
                }
                if (kind.body) {
                    clo.body = lower_block(src, std::move(*kind.body), diags);
                }
                out->kind = std::move(clo);
            } else if constexpr (std::is_same_v<K, ExprCast>) {
                out->kind = HirCast{
                    lower_expr(src, std::move(kind.expr), diags),
                    lower_type(kind.ty),
                };
            } else if constexpr (std::is_same_v<K, ExprUnwrap>) {
                out->kind = HirUnwrap{lower_expr(src, std::move(kind.expr), diags)};
            } else if constexpr (std::is_same_v<K, ExprCoalesce>) {
                out->kind = HirCoalesce{
                    lower_expr(src, std::move(kind.lhs), diags),
                    lower_expr(src, std::move(kind.rhs), diags),
                };
            }
        },
        expr->kind);

    return out;
}

Type parse_return_ty(const FnDecl& fn) {
    if (!fn.return_ty) {
        return Type::unit();
    }
    return lower_type(*fn.return_ty);
}

HirFn lower_fn(const Source& src, FnDecl& fn, DiagnosticEngine& diags, std::string self_ty = {}) {
    HirFn hfn;
    hfn.pub = fn.pub;
    hfn.is_extern = fn.is_extern;
    hfn.c_abi = fn.abi == Abi::C;
    hfn.self_kind = self_kind_from(fn.self_param);
    hfn.self_ty = std::move(self_ty);
    hfn.name = std::move(fn.name);
    hfn.offset = fn.offset;
    hfn.return_ty = parse_return_ty(fn);
    hfn.type_params.reserve(fn.type_params.size());
    for (auto& tp : fn.type_params) {
        hfn.type_params.push_back(HirTypeParam{std::move(tp.name), std::move(tp.bound)});
    }
    hfn.params.reserve(fn.params.size());

    for (auto& p : fn.params) {
        HirParam hp;
        hp.name = std::move(p.name);
        hp.offset = p.offset;
        hp.ty = lower_type(p.ty);
        hfn.params.push_back(std::move(hp));
    }

    if (!fn.is_extern) {
        hfn.body = lower_block(src, std::move(fn.body), diags);
    }
    return hfn;
}

HirStruct lower_struct(const Source& src, StructDecl& st, DiagnosticEngine& diags) {
    HirStruct out;
    out.pub = st.pub;
    out.is_extern = st.is_extern;
    out.opaque = st.opaque;
    out.name = std::move(st.name);
    out.offset = st.offset;
    out.fields.reserve(st.fields.size());

    for (auto& field : st.fields) {
        HirField hf;
        hf.mut = field.mut;
        hf.name = std::move(field.name);
        hf.offset = field.offset;
        hf.ty = lower_type(field.ty);
        out.fields.push_back(std::move(hf));
    }
    return out;
}

HirImpl lower_impl(const Source& src, ImplDecl& impl, DiagnosticEngine& diags) {
    HirImpl out;
    out.trait_name = impl.trait_name;
    out.type_name = impl.type_name;
    out.offset = impl.offset;
    out.methods.reserve(impl.methods.size());

    for (auto& method : impl.methods) {
        out.methods.push_back(lower_fn(src, method, diags, impl.type_name));
    }
    return out;
}

HirCEnum lower_c_enum(EnumDecl& en) {
    HirCEnum out;
    out.pub = en.pub;
    out.name = std::move(en.name);
    out.offset = en.offset;
    out.members.reserve(en.members.size());
    for (auto& member : en.members) {
        HirCEnumMember m;
        m.name = std::move(member.name);
        m.value = member.value.value_or(0);
        m.offset = member.offset;
        out.members.push_back(std::move(m));
    }
    return out;
}

HirVariant lower_variant(const Source& src, VariantTypeDecl& en, DiagnosticEngine& diags) {
    HirVariant out;
    out.pub = en.pub;
    out.name = std::move(en.name);
    out.offset = en.offset;
    out.variants.reserve(en.variants.size());

    for (auto& variant : en.variants) {
        HirEnumVariant hv;
        hv.name = std::move(variant.name);
        hv.tuple = variant.tuple;
        hv.offset = variant.offset;
        hv.fields.reserve(variant.fields.size());
        for (auto& field : variant.fields) {
            HirField hf;
            hf.mut = field.mut;
            hf.name = std::move(field.name);
            hf.offset = field.offset;
            hf.ty = lower_type(field.ty);
            hv.fields.push_back(std::move(hf));
        }
        out.variants.push_back(std::move(hv));
    }
    return out;
}

}  // namespace

HirModule lower_file(const Source& src, AstFile ast, DiagnosticEngine& diags) {
    HirModule mod;
    mod.uses.reserve(ast.uses.size());
    for (auto& u : ast.uses) {
        HirUse hu;
        hu.path = std::move(u.path);
        hu.glob = u.glob;
        hu.offset = u.offset;
        mod.uses.push_back(std::move(hu));
    }
    mod.mods.reserve(ast.mods.size());
    for (auto& nested : ast.mods) {
        if (!nested.body) {
            diags.error(src, nested.offset, "module '" + nested.name + "' is missing a body");
            continue;
        }
        const Source& child_src = nested.source ? *nested.source : src;
        HirModule child = lower_file(child_src, std::move(*nested.body), diags);
        child.name = std::move(nested.name);
        child.source = nested.source;
        mod.mods.push_back(std::move(child));
    }
    mod.statics.reserve(ast.statics.size());
    for (auto& st : ast.statics) {
        HirStatic hs;
        hs.pub = st.pub;
        hs.mut = st.mut;
        hs.is_extern = st.is_extern;
        hs.name = std::move(st.name);
        hs.offset = st.offset;
        if (st.ty) {
            hs.ty = lower_type(*st.ty);
        }
        if (st.init) {
            hs.init = lower_expr(src, std::move(st.init), diags);
        }
        mod.statics.push_back(std::move(hs));
    }
    mod.traits.reserve(ast.traits.size());
    for (auto& tr : ast.traits) {
        HirTrait ht;
        ht.pub = tr.pub;
        ht.name = std::move(tr.name);
        ht.offset = tr.offset;
        for (auto& m : tr.methods) {
            HirTraitMethod hm;
            hm.self_kind = self_kind_from(m.self_param);
            hm.name = std::move(m.name);
            hm.return_ty = m.return_ty ? lower_type(*m.return_ty) : Type::unit();
            for (auto& p : m.params) {
                HirParam hp;
                hp.name = std::move(p.name);
                hp.offset = p.offset;
                hp.ty = lower_type(p.ty);
                hm.params.push_back(std::move(hp));
            }
            ht.methods.push_back(std::move(hm));
        }
        mod.traits.push_back(std::move(ht));
    }
    mod.structs.reserve(ast.structs.size());
    mod.enums.reserve(ast.enums.size());
    mod.variants.reserve(ast.variants.size());
    mod.impls.reserve(ast.impls.size());
    mod.functions.reserve(ast.functions.size());

    for (auto& st : ast.structs) {
        mod.structs.push_back(lower_struct(src, st, diags));
    }
    for (auto& en : ast.enums) {
        mod.enums.push_back(lower_c_enum(en));
    }
    for (auto& var : ast.variants) {
        mod.variants.push_back(lower_variant(src, var, diags));
    }
    for (auto& impl : ast.impls) {
        mod.impls.push_back(lower_impl(src, impl, diags));
    }
    for (auto& fn : ast.functions) {
        mod.functions.push_back(lower_fn(src, fn, diags));
    }
    return mod;
}

HirModule lower(const Source& src, AstFile ast, DiagnosticEngine& diags) {
    HirModule mod = lower_file(src, std::move(ast), diags);
    mod.source = &src;
    return mod;
}

}  // namespace qpc

#include "compiler/typeck/type_checker.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace qpc::detail {

Type TypeChecker::check_expr(HirExpr& expr) {
        Type ty = Type::error();
        std::visit(
            [&](auto&& kind) {
                using K = std::decay_t<decltype(kind)>;
                if constexpr (std::is_same_v<K, HirLitInt>) {
                    ty = kind.unsuffixed ? Type::i32() : kind.ty;
                    kind.ty = ty;
                } else if constexpr (std::is_same_v<K, HirLitFloat>) {
                    ty = kind.unsuffixed ? Type::f32() : kind.ty;
                    kind.ty = ty;
                } else if constexpr (std::is_same_v<K, HirLitBool>) {
                    ty = Type::boolean();
                } else if constexpr (std::is_same_v<K, HirLitChar>) {
                    ty = Type::char_();
                } else if constexpr (std::is_same_v<K, HirLitString>) {
                    ty = Type::string();
                } else if constexpr (std::is_same_v<K, HirLitNull>) {
                    ty = Type::unknown();
                } else if constexpr (std::is_same_v<K, HirVar>) {
                    ty = check_var(kind, expr);
                } else if constexpr (std::is_same_v<K, HirBinary>) {
                    ty = check_binop(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirUnary>) {
                    ty = check_unary(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirCall>) {
                    ty = check_call(kind, expr);
                } else if constexpr (std::is_same_v<K, HirAssign>) {
                    ty = check_assign(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirFieldAccess>) {
                    ty = check_field(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirStructLit>) {
                    ty = check_struct_lit(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirEnumLit>) {
                    ty = check_enum_lit(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirMethodCall>) {
                    ty = check_method_call(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirFieldAssign>) {
                    ty = check_field_assign(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirIndex>) {
                    ty = check_index(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirIndexAssign>) {
                    ty = check_index_assign(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirListLit>) {
                    ty = check_list_lit(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirTupleLit>) {
                    ty = check_tuple_lit(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirDictLit>) {
                    ty = check_dict_lit(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirMatch>) {
                    ty = check_match(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirIf>) {
                    ty = check_if(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirRange>) {
                    ty = check_range(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirClosure>) {
                    ty = check_closure(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirCast>) {
                    ty = check_cast(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirUnwrap>) {
                    ty = check_unwrap(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirNew>) {
                    ty = check_new(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirCoalesce>) {
                    ty = check_coalesce(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirTry>) {
                    ty = check_try(kind, expr.offset);
                }
            },
            expr.kind);
        expr.ty = ty;
        return ty;
    }

Type TypeChecker::check_path_var(HirVar& var, HirExpr& expr) {
        const auto pos = var.name.rfind("::");
        if (pos == std::string::npos) {
            return Type::error();
        }
        const std::string head = var.name.substr(0, pos);
        const std::string tail = var.name.substr(pos + 2);
        if (tail.empty() || tail.find("::") != std::string::npos) {
            // only Type::Member for enums/variants; deeper paths are statics
        } else {
            const std::string type_key = lookup_named(head);
            if (auto c_it = c_enums_.find(type_key); c_it != c_enums_.end()) {
                if (!find_member(c_it->second, tail)) {
                    error(expr.offset, "unknown member '" + tail + "' on '" + type_key + "'");
                    return Type::error();
                }
                HirEnumLit lit;
                lit.enum_name = type_key;
                lit.variant = tail;
                expr.kind = std::move(lit);
                return check_enum_lit(std::get<HirEnumLit>(expr.kind), expr.offset);
            }
            if (auto v_it = variants_.find(type_key); v_it != variants_.end()) {
                const VariantInfo* v = find_variant(v_it->second, tail);
                if (!v) {
                    error(expr.offset, "unknown variant '" + tail + "' on '" + type_key + "'");
                    return Type::error();
                }
                if (!v->fields.empty()) {
                    error(expr.offset, "variant '" + tail + "' requires fields");
                    return Type::error();
                }
                HirEnumLit lit;
                lit.enum_name = type_key;
                lit.variant = tail;
                expr.kind = std::move(lit);
                return check_enum_lit(std::get<HirEnumLit>(expr.kind), expr.offset);
            }
        }
        if (auto* b = lookup_static(var.name)) {
            const auto leaf = path_leaf(var.name);
            if (statics_.contains(leaf)) {
                var.name = leaf;
            }
            return b->ty;
        }
        if (bind_fn_value(var, expr, nullptr)) {
            return expr.ty;
        }
        error(expr.offset, "unknown identifier '" + var.name + "'");
        return Type::error();
    }

Type TypeChecker::check_var(HirVar& var, HirExpr& expr) {
        if (name_has_path(var.name)) {
            return check_path_var(var, expr);
        }
        if (auto* b = lookup_binding(var.name)) {
            return b->ty;
        }
        if (bind_fn_value(var, expr, nullptr)) {
            return expr.ty;
        }
        error(expr.offset, "unknown identifier '" + var.name + "'");
        return Type::error();
    }

bool TypeChecker::bind_fn_value(HirVar& var, HirExpr& expr, const Type* expected) {
        struct Cand {
            std::vector<HirTypeParam> type_params;
            std::vector<Type> params;
            Type ret;
        };
        std::vector<Cand> cands;
        bool saw_self_method = false;
        if (name_has_path(var.name)) {
            const auto pos = var.name.rfind("::");
            if (pos == std::string::npos || pos == 0 || pos + 2 >= var.name.size()) {
                return false;
            }
            const std::string head = var.name.substr(0, pos);
            const std::string tail = var.name.substr(pos + 2);
            if (tail.find("::") != std::string::npos) {
                return false;
            }
            auto type_it = methods_.find(lookup_named(head));
            if (type_it == methods_.end()) {
                type_it = methods_.find(head);
            }
            if (type_it == methods_.end()) {
                return false;
            }
            auto method_it = type_it->second.find(tail);
            if (method_it == type_it->second.end()) {
                return false;
            }
            for (const auto& sig : method_it->second) {
                if (sig.self_kind != SelfKind::None) {
                    saw_self_method = true;
                    continue;
                }
                cands.push_back(Cand{sig.type_params, sig.params, sig.ret});
            }
            if (cands.empty()) {
                if (saw_self_method) {
                    error(expr.offset, "cannot use method '" + var.name +
                                           "' as a value; wrap it in a closure");
                    expr.ty = Type::error();
                    return true;
                }
                return false;
            }
        } else {
            auto it = sigs_.find(var.name);
            if (it == sigs_.end()) {
                return false;
            }
            for (const auto& sig : it->second) {
                cands.push_back(Cand{sig.type_params, sig.params, sig.ret});
            }
        }

        auto matches = [&](const Cand& cand, std::vector<Type>& targs) -> bool {
            if (expected == nullptr || expected->kind != TypeKind::Fn || expected->args.empty()) {
                return cand.type_params.empty();
            }
            if (cand.params.size() + 1 != expected->args.size()) {
                return false;
            }
            if (cand.type_params.empty()) {
                targs.clear();
                return Type::fn(cand.params, cand.ret) == *expected;
            }
            std::unordered_map<std::string, Type> mapping;
            for (std::size_t i = 0; i < cand.params.size(); ++i) {
                if (!unify_type(cand.params[i], expected->args[i], cand.type_params, mapping)) {
                    return false;
                }
            }
            if (!unify_type(cand.ret, expected->args.back(), cand.type_params, mapping)) {
                return false;
            }
            targs.clear();
            targs.reserve(cand.type_params.size());
            for (const auto& tp : cand.type_params) {
                auto mit = mapping.find(tp.name);
                if (mit == mapping.end() || mit->second.kind == TypeKind::Unknown ||
                    mit->second.kind == TypeKind::Error) {
                    return false;
                }
                targs.push_back(mit->second);
            }
            return true;
        };

        std::vector<std::pair<Cand, std::vector<Type>>> fits;
        for (const auto& cand : cands) {
            std::vector<Type> targs;
            if (matches(cand, targs)) {
                fits.emplace_back(cand, std::move(targs));
            }
        }
        if (expected == nullptr) {
            if (fits.size() == 1) {
                var.fn_value = true;
                var.type_args = std::move(fits[0].second);
                expr.ty = Type::fn(fits[0].first.params, fits[0].first.ret);
                return true;
            }
            if (!cands.empty()) {
                var.fn_value = true;
                expr.ty = Type::unknown();
                return true;
            }
            return false;
        }
        if (fits.size() == 1) {
            var.fn_value = true;
            var.type_args = std::move(fits[0].second);
            expr.ty = *expected;
            return true;
        }
        if (fits.size() > 1) {
            error(expr.offset, "ambiguous function value '" + var.name + "'");
            expr.ty = Type::error();
            return true;
        }
        if (!cands.empty()) {
            return false;
        }
        return false;
    }

Type TypeChecker::subst_type(Type t, const std::unordered_map<std::string, Type>& mapping) const {
        if (t.kind == TypeKind::Named) {
            if (auto it = mapping.find(t.name); it != mapping.end()) {
                return it->second;
            }
            return t;
        }
        for (auto& arg : t.args) {
            arg = subst_type(std::move(arg), mapping);
        }
        return t;
    }

bool TypeChecker::unify_type(const Type& pattern, const Type& actual, const std::vector<HirTypeParam>& tps,
 std::unordered_map<std::string, Type>& mapping) {
        const Type pat = subst_type(pattern, mapping);
        if (pat.kind == TypeKind::Named && is_generic_param(tps, pat.name)) {
            if (actual.kind == TypeKind::Unknown || actual.kind == TypeKind::Error) {
                return false;
            }
            mapping[pat.name] = actual;
            return true;
        }
        if (pat.kind != actual.kind || pat.name != actual.name || pat.size != actual.size ||
            pat.args.size() != actual.args.size()) {
            return false;
        }
        for (std::size_t i = 0; i < pat.args.size(); ++i) {
            if (!unify_type(pat.args[i], actual.args[i], tps, mapping)) {
                return false;
            }
        }
        return true;
    }

void TypeChecker::check_type_arg_bounds(const std::vector<HirTypeParam>& tps, const std::vector<Type>& args,
 std::size_t offset) {
        auto check_one = [&](const Type& arg, const std::optional<std::string>& bound) {
            if (!bound) {
                return;
            }
            const std::string& b = *bound;
            bool ok = false;
            if (arg.kind == TypeKind::Named) {
                const auto type_key = lookup_named(arg.name);
                if (auto it = trait_impls_.find(type_key); it != trait_impls_.end()) {
                    ok = it->second.contains(b);
                }
                if (!ok) {
                    if (auto it = trait_impls_.find(arg.name); it != trait_impls_.end()) {
                        ok = it->second.contains(b);
                    }
                }
                if (!ok) {
                    if (auto it = op_impls_.find(type_key); it != op_impls_.end()) {
                        ok = it->second.contains(b);
                    }
                }
                if (!ok) {
                    if (auto it = op_impls_.find(arg.name); it != op_impls_.end()) {
                        ok = it->second.contains(b);
                    }
                }
            }
            if (!ok) {
                error(offset, "type '" + type_name(arg) + "' does not implement '" + b + "'");
            }
        };
        const std::size_t prefix = pack_prefix(tps);
        for (std::size_t i = 0; i < prefix && i < args.size(); ++i) {
            check_one(args[i], tps[i].bound);
        }
        if (last_is_pack(tps)) {
            for (std::size_t i = prefix; i < args.size(); ++i) {
                check_one(args[i], tps.back().bound);
            }
        }
    }

void TypeChecker::bind_type_args(const std::vector<HirTypeParam>& tps, std::vector<Type>& args, std::size_t offset,
 std::unordered_map<std::string, Type>& mapping) {
        if (!type_arg_count_ok(tps, args.size())) {
            error(offset, "expects " +
                              (last_is_pack(tps) ? "at least " + std::to_string(pack_prefix(tps))
                                                 : std::to_string(tps.size())) +
                              " type argument(s), got " + std::to_string(args.size()));
        }
        const std::size_t prefix = pack_prefix(tps);
        const std::size_t n = std::min(args.size(), prefix);
        for (std::size_t i = 0; i < n; ++i) {
            resolve_type(args[i], offset);
            mapping[tps[i].name] = args[i];
        }
        for (std::size_t i = n; i < args.size(); ++i) {
            resolve_type(args[i], offset);
        }
        check_type_arg_bounds(tps, args, offset);
    }

void TypeChecker::infer_type_args(const std::vector<HirTypeParam>& tps, std::vector<Type>& type_args,
 const std::vector<Type>& params, std::vector<HirExprPtr>& args, std::size_t offset,
 std::unordered_map<std::string, Type>& mapping) {
        const std::size_t n = std::min(args.size(), params.size());
        for (std::size_t i = 0; i < n; ++i) {
            if (unify_type(params[i], args[i]->ty, tps, mapping)) {
                continue;
            }
            if (params[i].kind == TypeKind::Named && is_generic_param(tps, params[i].name)) {
                auto it = mapping.find(params[i].name);
                if (it != mapping.end() && coerce_lit(*args[i], it->second)) {
                    continue;
                }
            }
            error(offset, "cannot infer type arguments");
        }
        type_args.clear();
        type_args.reserve(tps.size());
        for (const auto& tp : tps) {
            if (tp.pack) {
                error(offset, "cannot infer type-parameter pack '" + tp.name + "', write type arguments");
                continue;
            }
            auto it = mapping.find(tp.name);
            if (it == mapping.end() || it->second.kind == TypeKind::Unknown ||
                it->second.kind == TypeKind::Error) {
                error(offset, "cannot infer type argument '" + tp.name + "'");
                mapping[tp.name] = Type::error();
                type_args.push_back(Type::error());
            } else {
                type_args.push_back(it->second);
            }
        }
        check_type_arg_bounds(tps, type_args, offset);
    }

void TypeChecker::bind_or_infer_type_args(const std::vector<HirTypeParam>& tps, std::vector<Type>& type_args,
 const std::vector<Type>& params, std::vector<HirExprPtr>& args,
 std::size_t offset, std::unordered_map<std::string, Type>& mapping) {
        if (type_args.empty() && !tps.empty()) {
            infer_type_args(tps, type_args, params, args, offset, mapping);
            return;
        }
        bind_type_args(tps, type_args, offset, mapping);
    }

Type TypeChecker::check_if(HirIf& iff, std::size_t offset) {
        Type cond_ty = Type::error();
        if (!iff.let_name.empty()) {
            cond_ty = check_expr(*iff.cond);
            if (cond_ty.kind != TypeKind::Nullable && cond_ty != Type::error()) {
                error(iff.cond->offset, "if-let requires a '" + type_name(cond_ty) + "?' value");
            }
        } else {
            expect_expr(*iff.cond, Type::boolean(), iff.cond->offset, "if condition");
        }
        push_scope();
        if (!iff.let_name.empty() && cond_ty.kind == TypeKind::Nullable) {
            declare(iff.let_name, Binding{cond_ty.elem(), false}, iff.cond->offset);
        }
        for (auto& s : iff.then_stmts) {
            check_stmt(*s);
        }
        Type then_ty = Type::unit();
        if (iff.then_tail) {
            then_ty = check_expr(*iff.then_tail);
        } else if (!iff.then_stmts.empty() &&
                   std::holds_alternative<HirReturn>(iff.then_stmts.back()->kind)) {
            then_ty = Type::never();
        }
        pop_scope();

        if (!iff.else_expr) {
            if (then_ty.kind == TypeKind::Never) {
                return then_ty;
            }
            if (then_ty != Type::unit() && then_ty != Type::error()) {
                if (std::holds_alternative<HirLitBool>(iff.cond->kind) &&
                    std::get<HirLitBool>(iff.cond->kind).value) {
                    return then_ty;
                }
                error(offset, "if expression is missing an else branch");
            }
            return Type::unit();
        }

        Type else_ty = check_expr(*iff.else_expr);
        if (then_ty == Type::error() || else_ty == Type::error()) {
            return Type::error();
        }
        if (then_ty.kind == TypeKind::Never) {
            return else_ty;
        }
        if (else_ty.kind == TypeKind::Never) {
            return then_ty;
        }
        if (then_ty != else_ty) {
            if (iff.then_tail && coerce_lit(*iff.then_tail, else_ty)) {
                then_ty = else_ty;
            } else if (coerce_lit(*iff.else_expr, then_ty)) {
                else_ty = then_ty;
            } else if (else_ty.kind == TypeKind::Nullable && then_ty == else_ty.elem() && iff.then_tail) {
                iff.then_tail->coerce_nullable = true;
                iff.then_tail->ty = else_ty;
                then_ty = else_ty;
            } else if (then_ty.kind == TypeKind::Nullable && else_ty == then_ty.elem()) {
                iff.else_expr->coerce_nullable = true;
                iff.else_expr->ty = then_ty;
                else_ty = then_ty;
            } else if (iff.then_tail && coerce_null_to_nullable(*iff.else_expr, Type::nullable(then_ty))) {
                iff.then_tail->coerce_nullable = true;
                iff.then_tail->ty = Type::nullable(then_ty);
                then_ty = iff.then_tail->ty;
                else_ty = then_ty;
            } else if (iff.then_tail && coerce_null_to_nullable(*iff.then_tail, Type::nullable(else_ty))) {
                iff.else_expr->coerce_nullable = true;
                iff.else_expr->ty = Type::nullable(else_ty);
                else_ty = iff.else_expr->ty;
                then_ty = else_ty;
            }
        }
        if (then_ty != else_ty) {
            error(offset, "if branches have types '" + type_name(then_ty) + "' and '" + type_name(else_ty) +
                              "'");
            return Type::error();
        }
        return then_ty;
    }

Type TypeChecker::check_range(HirRange& range, std::size_t offset) {
        Type lo = check_expr(*range.start);
        Type hi = check_expr(*range.end);
        if (lo == Type::error() || hi == Type::error()) {
            return Type::error();
        }
        if (lo != hi) {
            if (coerce_lit(*range.start, hi)) {
                lo = hi;
            } else if (coerce_lit(*range.end, lo)) {
                hi = lo;
            }
        }
        if (lo != hi || !is_int(lo)) {
            error(offset, "range bounds must be integers of the same type");
            return Type::error();
        }
        return lo;
    }

Type TypeChecker::check_closure(HirClosure& clo, std::size_t offset) {
        const Type saved_ret = current_ret_;
        push_scope();
        closure_frames_.push_back({clo.by_ref, scopes_.size() - 1});
        std::vector<Type> params;
        for (auto& p : clo.params) {
            resolve_type(p.ty, p.offset);
            declare(p.name, Binding{p.ty, true}, p.offset);
            params.push_back(p.ty);
        }
        if (clo.return_ty.kind != TypeKind::Unknown) {
            resolve_type(clo.return_ty, offset);
            current_ret_ = clo.return_ty;
        } else {
            current_ret_ = Type::unknown();
        }
        for (auto& stmt : clo.body.stmts) {
            check_stmt(*stmt);
        }
        Type ret = Type::unit();
        if (clo.body.tail) {
            if (clo.return_ty.kind != TypeKind::Unknown) {
                expect_expr(*clo.body.tail, clo.return_ty, clo.body.tail->offset, "closure body");
                ret = clo.return_ty;
            } else {
                ret = check_expr(*clo.body.tail);
                clo.return_ty = ret;
            }
        } else if (clo.return_ty.kind != TypeKind::Unknown && clo.return_ty != Type::unit()) {
            if (!ends_with_return(clo.body)) {
                error(offset, "closure is missing a return value");
            }
            ret = clo.return_ty;
        } else if (clo.return_ty.kind == TypeKind::Unknown) {
            ret = current_ret_.kind == TypeKind::Unknown ? Type::unit() : current_ret_;
            clo.return_ty = ret;
        } else {
            ret = clo.return_ty;
        }
        pop_scope();
        closure_frames_.pop_back();
        current_ret_ = saved_ret;
        return Type::fn(std::move(params), std::move(ret));
    }

bool TypeChecker::can_cast(const Type& from, const Type& to) const {
        if (from == to || from == Type::error() || to == Type::error()) {
            return true;
        }
        if (is_numeric(from) && is_numeric(to)) {
            return true;
        }
        if (from == Type::boolean() && is_int(to)) {
            return true;
        }
        if (from == Type::char_() && is_int(to)) {
            return true;
        }
        if (is_int(from) && to == Type::char_()) {
            return true;
        }
        if (from.kind == TypeKind::Named && c_enums_.contains(from.name) && is_int(to)) {
            return true;
        }
        return false;
    }

Type TypeChecker::check_cast(HirCast& c, std::size_t offset) {
        const Type from = check_expr(*c.expr);
        if (!can_cast(from, c.ty)) {
            error(offset, "cannot cast '" + type_name(from) + "' as '" + type_name(c.ty) + "'");
            return Type::error();
        }
        return c.ty;
    }

Type TypeChecker::check_unwrap(HirUnwrap& un, std::size_t offset) {
        const Type inner = check_expr(*un.expr);
        if (inner.kind == TypeKind::Nullable) {
            return inner.elem();
        }
        if (inner != Type::error()) {
            error(offset, "unwrap '!' requires a '" + type_name(inner) + "?' value");
        }
        return Type::error();
    }

Type TypeChecker::check_new(HirNew& n, std::size_t offset) {
        HirStructLit lit;
        lit.name = n.name;
        lit.type_args = std::move(n.type_args);
        lit.fields = std::move(n.fields);
        const Type inner = check_struct_lit(lit, offset);
        n.type_args = lit.type_args;
        n.fields = std::move(lit.fields);
        if (inner.kind == TypeKind::Error) {
            return Type::error();
        }
        return Type::nullable(inner);
    }

Type TypeChecker::check_coalesce(HirCoalesce& c, std::size_t offset) {
        const Type lhs = check_expr(*c.lhs);
        Type rhs = check_expr(*c.rhs);
        if (lhs.kind != TypeKind::Nullable) {
            if (lhs != Type::error()) {
                error(offset, "'??' requires a '" + type_name(lhs) + "?' value on the left");
            }
            return Type::error();
        }
        const Type inner = lhs.elem();
        if (rhs != inner && rhs != Type::error()) {
            if (coerce_lit(*c.rhs, inner)) {
                rhs = inner;
            }
        }
        if (rhs != inner && rhs != Type::error()) {
            error(offset, "'??' fallback has type '" + type_name(rhs) + "', expected '" + type_name(inner) +
                              "'");
            return Type::error();
        }
        return inner;
    }

Type TypeChecker::check_try(HirTry& t, std::size_t offset) {
        const Type inner = check_expr(*t.expr);
        if (inner.kind != TypeKind::Nullable) {
            if (inner != Type::error()) {
                error(offset, "'?' requires a '" + type_name(inner) + "?' value");
            }
            return Type::error();
        }
        if (current_ret_.kind == TypeKind::Unknown) {
            current_ret_ = inner;
        } else if (current_ret_.kind != TypeKind::Nullable) {
            error(offset, "'?' requires a function that returns '" + type_name(inner) + "'");
            return Type::error();
        } else if (current_ret_ != inner) {
            error(offset, "'?' has type '" + type_name(inner) + "', but the function returns '" +
                              type_name(current_ret_) + "'");
            return Type::error();
        }
        return inner.elem();
    }

Type TypeChecker::check_unary(HirUnary& un, std::size_t offset) {
        const Type inner = check_expr(*un.operand);
        if (un.op == UnOp::Not) {
            expect_type(inner, Type::boolean(), offset, "operand");
            return Type::boolean();
        }
        if (is_signed_int(inner) || is_float(inner)) {
            return inner;
        }
        if (inner.kind == TypeKind::Named) {
            auto type_it = op_impls_.find(lookup_named(inner.name));
            if (type_it == op_impls_.end()) {
                type_it = op_impls_.find(inner.name);
            }
            if (type_it != op_impls_.end()) {
                auto trait_it = type_it->second.find("Neg");
                if (trait_it != type_it->second.end()) {
                    return trait_it->second.ret;
                }
            }
            error(offset, "type '" + inner.name + "' does not implement Neg");
            return Type::error();
        }
        if (inner != Type::error()) {
            error(offset, "unary '-' requires a signed integer, float, or Neg impl");
        }
        return Type::error();
    }

bool TypeChecker::coerce_lit(HirExpr& expr, Type expected) {
        if (auto* lit = std::get_if<HirLitInt>(&expr.kind)) {
            if (lit->unsuffixed && is_int(expected) && int_fits(lit->value, expected)) {
                lit->ty = expected;
                expr.ty = expected;
                return true;
            }
        }
        if (auto* lit = std::get_if<HirLitFloat>(&expr.kind)) {
            if (lit->unsuffixed && is_float(expected)) {
                lit->ty = expected;
                expr.ty = expected;
                return true;
            }
        }
        if (std::holds_alternative<HirLitNull>(expr.kind) && expected.kind == TypeKind::Nullable) {
            expr.ty = expected;
            return true;
        }
        return false;
    }

bool TypeChecker::coerce_null_to_nullable(HirExpr& expr, Type want) {
        if (want.kind != TypeKind::Nullable) {
            return false;
        }
        if (coerce_lit(expr, want)) {
            return true;
        }
        if (auto* eif = std::get_if<HirIf>(&expr.kind)) {
            if (eif->then_tail && !eif->else_expr && coerce_lit(*eif->then_tail, want)) {
                expr.ty = want;
                return true;
            }
        }
        return false;
    }

Type TypeChecker::check_binop(HirBinary& bin, std::size_t offset) {
        Type lhs = check_expr(*bin.lhs);
        Type rhs = check_expr(*bin.rhs);
        if (lhs == Type::error() || rhs == Type::error()) {
            return Type::error();
        }

        if (lhs != rhs) {
            if (coerce_lit(*bin.lhs, rhs)) {
                lhs = rhs;
            } else if (coerce_lit(*bin.rhs, lhs)) {
                rhs = lhs;
            }
        }

        if (is_eq_op(bin.op) && (lhs.kind == TypeKind::Nullable || rhs.kind == TypeKind::Nullable ||
                                 std::holds_alternative<HirLitNull>(bin.lhs->kind) ||
                                 std::holds_alternative<HirLitNull>(bin.rhs->kind))) {
            if (lhs.kind == TypeKind::Unknown && rhs.kind == TypeKind::Nullable) {
                bin.lhs->ty = rhs;
                lhs = rhs;
            }
            if (rhs.kind == TypeKind::Unknown && lhs.kind == TypeKind::Nullable) {
                bin.rhs->ty = lhs;
                rhs = lhs;
            }
            if (lhs.kind == TypeKind::Nullable && rhs.kind == TypeKind::Nullable && lhs == rhs) {
                return Type::boolean();
            }
            error(offset, "cannot compare '" + type_name(lhs) + "' and '" + type_name(rhs) + "'");
            return Type::error();
        }

        if (bin.op == BinOp::Add && lhs == Type::string() && rhs == Type::string()) {
            return Type::string();
        }

        if (is_logic_op(bin.op)) {
            expect_type(lhs, Type::boolean(), bin.lhs->offset, "operand");
            expect_type(rhs, Type::boolean(), bin.rhs->offset, "operand");
            return Type::boolean();
        }

        if (is_eq_op(bin.op) || is_ord_op(bin.op)) {
            if (lhs != rhs) {
                error(offset, "cannot compare '" + type_name(lhs) + "' and '" + type_name(rhs) + "'");
                return Type::error();
            }
            const bool comparable =
                is_numeric(lhs) || lhs == Type::boolean() || lhs == Type::char_() || lhs == Type::string() ||
                lhs.kind == TypeKind::Nullable ||
                (lhs.kind == TypeKind::Named && c_enums_.contains(lhs.name));
            if (is_ord_op(bin.op) && !is_numeric(lhs)) {
                error(offset, "cannot ordered-compare '" + type_name(lhs) + "'");
                return Type::error();
            }
            if (!comparable) {
                error(offset, "cannot compare '" + type_name(lhs) + "'");
                return Type::error();
            }
            return Type::boolean();
        }

        if (lhs.kind == TypeKind::Named) {
            auto type_it = op_impls_.find(lookup_named(lhs.name));
            if (type_it == op_impls_.end()) {
                type_it = op_impls_.find(lhs.name);
            }
            if (type_it != op_impls_.end()) {
                auto trait_it = type_it->second.find(binop_trait(bin.op));
                if (trait_it != type_it->second.end()) {
                    const MethodSig& sig = trait_it->second;
                    if (sig.params.size() != 1) {
                        error(offset, "operator impl expects one rhs argument");
                        return Type::error();
                    }
                    expect_type(rhs, sig.params[0], bin.rhs->offset, "operand");
                    return sig.ret;
                }
            }
            error(offset, "type '" + lhs.name + "' does not implement " + binop_trait(bin.op));
            return Type::error();
        }

        if (lhs != rhs) {
            error(offset, "cannot apply operator to '" + type_name(lhs) + "' and '" + type_name(rhs) + "'");
            return Type::error();
        }
        if (bin.op == BinOp::Mod) {
            if (!is_int(lhs)) {
                error(offset, "'%' requires integer operands");
                return Type::error();
            }
            return lhs;
        }
        if (!is_numeric(lhs)) {
            error(offset, "arithmetic requires a numeric type");
            return Type::error();
        }
        return lhs;
    }

Type TypeChecker::check_fn_value_call(const Type& fn_ty, HirCall& call, std::size_t offset, std::string callee) {
        const std::size_t nparams = fn_ty.args.size() - 1;
        if (call.args.size() != nparams) {
            error(offset, callee + " expects " + std::to_string(nparams) + " argument(s), got " +
                              std::to_string(call.args.size()));
        }
        const std::size_t n = std::min(call.args.size(), nparams);
        for (std::size_t i = 0; i < n; ++i) {
            expect_expr(*call.args[i], fn_ty.args[i], call.args[i]->offset, "argument");
        }
        for (std::size_t i = n; i < call.args.size(); ++i) {
            check_expr(*call.args[i]);
        }
        return fn_ty.args.back();
    }

Type TypeChecker::check_math_builtin(HirCall& call, std::size_t offset) {
        if (!is_math_builtin(call.callee)) {
            return Type::unknown();
        }
        if (!call.type_args.empty()) {
            error(offset, "math function '" + call.callee + "' cannot take type arguments");
            return Type::error();
        }
        const std::size_t want = is_binary_math(call.callee) ? 2u : 1u;
        for (auto& arg : call.args) {
            check_expr(*arg);
        }
        if (call.args.size() != want) {
            error(offset, "function '" + call.callee + "' expects " + std::to_string(want) +
                              " argument(s), got " + std::to_string(call.args.size()));
            return Type::error();
        }
        Type result = Type::f32();
        for (const auto& arg : call.args) {
            if (arg->ty.kind == TypeKind::F64) {
                result = Type::f64();
                break;
            }
        }
        for (auto& arg : call.args) {
            if (is_float(arg->ty) || arg->ty == Type::error()) {
                expect_expr(*arg, result, arg->offset, "argument");
                continue;
            }
            if (coerce_lit(*arg, result)) {
                continue;
            }
            error(arg->offset, "argument has type '" + type_name(arg->ty) + "', expected '" +
                                   type_name(result) + "'");
            return Type::error();
        }
        return result;
    }

bool TypeChecker::can_to_string(const Type& ty) {
        if (is_numeric(ty) || ty == Type::boolean() || ty == Type::char_() || ty == Type::string() ||
            ty == Type::error()) {
            return true;
        }
        if (ty.kind == TypeKind::Named) {
            return c_enums_.contains(lookup_named(ty.name)) || c_enums_.contains(ty.name);
        }
        return false;
    }

Type TypeChecker::check_to_string_builtin(HirCall& call, std::size_t offset) {
        if (call.callee != "to_string") {
            return Type::unknown();
        }
        if (!call.type_args.empty()) {
            error(offset, "'to_string' cannot take type arguments");
            return Type::error();
        }
        for (auto& arg : call.args) {
            check_expr(*arg);
        }
        if (call.args.size() != 1) {
            error(offset, "function 'to_string' expects 1 argument(s), got " +
                              std::to_string(call.args.size()));
            return Type::error();
        }
        if (!can_to_string(call.args[0]->ty)) {
            error(call.args[0]->offset,
                  "cannot convert '" + type_name(call.args[0]->ty) + "' to string");
            return Type::error();
        }
        return Type::string();
    }

Type TypeChecker::check_call(HirCall& call, HirExpr& expr) {
        const std::size_t offset = expr.offset;
        if (call.callee_expr) {
            const Type fn_ty = check_expr(*call.callee_expr);
            if (fn_ty.kind != TypeKind::Fn || fn_ty.args.empty()) {
                if (fn_ty != Type::error()) {
                    error(offset, "value of type '" + type_name(fn_ty) + "' is not callable");
                }
                for (auto& arg : call.args) {
                    check_expr(*arg);
                }
                return Type::error();
            }
            return check_fn_value_call(fn_ty, call, offset, "closure");
        }

        auto it = sigs_.find(call.callee);
        if (it == sigs_.end() && name_has_path(call.callee)) {
            it = sigs_.find(path_leaf(call.callee));
        }
        if (it == sigs_.end()) {
            if (auto* b = lookup(call.callee)) {
                if (b->ty.kind == TypeKind::Fn && !b->ty.args.empty()) {
                    return check_fn_value_call(b->ty, call, offset, "closure '" + call.callee + "'");
                }
                error(offset, "value of type '" + type_name(b->ty) + "' is not callable");
                for (auto& arg : call.args) {
                    check_expr(*arg);
                }
                return Type::error();
            }
            const Type to_string_ty = check_to_string_builtin(call, offset);
            if (to_string_ty.kind != TypeKind::Unknown) {
                return to_string_ty;
            }
            const Type math = check_math_builtin(call, offset);
            if (math.kind != TypeKind::Unknown) {
                return math;
            }
            if (name_has_path(call.callee)) {
                const auto pos = call.callee.rfind("::");
                HirEnumLit lit;
                lit.enum_name = call.callee.substr(0, pos);
                lit.variant = call.callee.substr(pos + 2);
                lit.tuple = true;
                lit.args = std::move(call.args);
                expr.kind = std::move(lit);
                return check_enum_lit(std::get<HirEnumLit>(expr.kind), offset);
            }
            error(offset, "unknown function '" + call.callee + "'");
            for (auto& arg : call.args) {
                check_expr(*arg);
            }
            return Type::error();
        }

        if (name_has_path(call.callee)) {
            call.callee = path_leaf(call.callee);
        }

        for (auto& arg : call.args) {
            check_expr(*arg);
        }

        std::unordered_map<std::string, Type> mapping;
        const FnSig* chosen = resolve_fn_overload(it->second, call.callee, call, offset, mapping);
        if (!chosen) {
            return Type::error();
        }
        for (std::size_t i = 0; i < call.args.size(); ++i) {
            expect_expr(*call.args[i], subst_type(chosen->params[i], mapping), call.args[i]->offset, "argument");
            if (i < chosen->param_mut.size() && chosen->param_mut[i] && !is_mut_place(*call.args[i])) {
                error(call.args[i]->offset, "mut argument must be a mutable place");
            }
        }
        if (!chosen->type_params.empty()) {
            check_type_arg_bounds(chosen->type_params, call.type_args, offset);
        }
        return subst_type(chosen->ret, mapping);
    }

Type TypeChecker::check_assign(HirAssign& as, std::size_t offset) {
        Binding* b = lookup_binding(as.name);
        const Type value_ty = check_expr(*as.value);

        if (!b) {
            error(offset, "unknown identifier '" + as.name + "'");
            return Type::error();
        }
        if (name_has_path(as.name)) {
            const auto leaf = path_leaf(as.name);
            if (statics_.contains(leaf)) {
                as.name = leaf;
            }
        }
        if (!b->mut) {
            error(offset, "cannot assign to immutable variable '" + as.name + "'");
        }
        if (!closure_frames_.empty() && !closure_frames_.back().first) {
            for (std::size_t i = 0; i < scopes_.size(); ++i) {
                if (scopes_[i].contains(as.name)) {
                    if (i < closure_frames_.back().second) {
                        error(offset, "cannot assign to captured variable '" + as.name +
                                          "'; use a 'ref' closure");
                    }
                    break;
                }
            }
        }

        expect_expr(*as.value, b->ty, as.value->offset, "assignment");
        return b->ty;
    }

bool TypeChecker::is_mut_place(const HirExpr& expr) {
        if (const auto* var = std::get_if<HirVar>(&expr.kind)) {
            if (auto* b = lookup_binding(var->name)) {
                return b->mut;
            }
            return false;
        }
        if (const auto* field = std::get_if<HirFieldAccess>(&expr.kind)) {
            return is_mut_place(*field->base);
        }
        if (const auto* index = std::get_if<HirIndex>(&expr.kind)) {
            return is_mut_place(*index->base);
        }
        return false;
    }

Type TypeChecker::check_field(HirFieldAccess& field, std::size_t offset) {
        const Type base_ty = check_expr(*field.base);
        Type struct_ty = base_ty;
        if (field.null_safe) {
            if (base_ty.kind != TypeKind::Nullable) {
                if (base_ty != Type::error()) {
                    error(offset, "'?.' requires a '" + type_name(base_ty) + "?' value");
                }
                return Type::error();
            }
            struct_ty = base_ty.elem();
        }
        if (struct_ty.kind == TypeKind::Tuple) {
            const auto idx = tuple_field_index(field.name);
            if (!idx || *idx >= struct_ty.args.size()) {
                error(offset, "unknown field '" + field.name + "' on '" + type_name(struct_ty) + "'");
                return Type::error();
            }
            Type field_ty = struct_ty.args[*idx];
            if (field.null_safe) {
                field.take_addr = field_ty.kind != TypeKind::Nullable;
                return as_nullable(std::move(field_ty));
            }
            return field_ty;
        }
        const StructInfo* st = struct_of(struct_ty);
        if (!st) {
            if (struct_ty != Type::error()) {
                error(offset, "field access requires a struct, found '" + type_name(struct_ty) + "'");
            }
            return Type::error();
        }
        if (st->opaque) {
            error(offset, "cannot access fields of opaque type '" + type_name(struct_ty) + "'");
            return Type::error();
        }

        const FieldInfo* info = find_field(*st, field.name);
        if (!info) {
            error(offset, "unknown field '" + field.name + "' on '" + type_name(struct_ty) + "'");
            return Type::error();
        }
        const auto mapping = struct_subst(*st, struct_ty);
        Type field_ty = subst_type(info->ty, mapping);
        if (field.null_safe) {
            field.take_addr = field_ty.kind != TypeKind::Nullable;
            return as_nullable(std::move(field_ty));
        }
        return field_ty;
    }

Type TypeChecker::check_struct_lit(HirStructLit& lit, std::size_t offset) {
        const auto key = lookup_named(lit.name);
        auto it = structs_.find(key);
        if (it == structs_.end()) {
            const auto sep = lit.name.rfind("::");
            if (sep != std::string::npos && sep > 0) {
                HirEnumLit el;
                el.enum_name = lit.name.substr(0, sep);
                el.variant = lit.name.substr(sep + 2);
                el.fields = std::move(lit.fields);
                const Type ty = check_enum_lit(el, offset);
                lit.fields = std::move(el.fields);
                return ty;
            }
            error(offset, "unknown struct '" + lit.name + "'");
            for (auto& field : lit.fields) {
                check_expr(*field.value);
            }
            return Type::error();
        }
        lit.name = key;

        const StructInfo& st = it->second;
        if (st.opaque) {
            error(offset, "cannot construct opaque type '" + lit.name + "'");
            for (auto& field : lit.fields) {
                check_expr(*field.value);
            }
            return Type::error();
        }
        std::vector<bool> seen(st.fields.size(), false);
        std::vector<std::size_t> field_index(lit.fields.size(), st.fields.size());

        for (std::size_t fi = 0; fi < lit.fields.size(); ++fi) {
            auto& field = lit.fields[fi];
            check_expr(*field.value);
            std::size_t index = st.fields.size();
            for (std::size_t i = 0; i < st.fields.size(); ++i) {
                if (st.fields[i].first == field.name) {
                    index = i;
                    break;
                }
            }
            if (index == st.fields.size()) {
                error(field.value->offset, "unknown field '" + field.name + "' on '" + lit.name + "'");
                continue;
            }
            if (seen[index]) {
                error(field.value->offset, "duplicate field '" + field.name + "'");
                continue;
            }
            seen[index] = true;
            field_index[fi] = index;
        }

        for (std::size_t i = 0; i < st.fields.size(); ++i) {
            if (!seen[i]) {
                error(offset, "missing field '" + st.fields[i].first + "' in '" + lit.name + "'");
            }
        }

        std::unordered_map<std::string, Type> mapping;
        if (!st.type_params.empty()) {
            if (lit.type_args.empty()) {
                for (std::size_t fi = 0; fi < lit.fields.size(); ++fi) {
                    if (field_index[fi] >= st.fields.size()) {
                        continue;
                    }
                    unify_type(st.fields[field_index[fi]].second.ty, lit.fields[fi].value->ty, st.type_params,
                               mapping);
                }
                lit.type_args.reserve(st.type_params.size());
                for (const auto& tp : st.type_params) {
                    if (tp.pack) {
                        error(offset, "cannot infer type-parameter pack '" + tp.name +
                                          "', write type arguments");
                        continue;
                    }
                    auto mit = mapping.find(tp.name);
                    if (mit == mapping.end() || mit->second.kind == TypeKind::Unknown ||
                        mit->second.kind == TypeKind::Error) {
                        error(offset, "cannot infer type argument '" + tp.name + "'");
                        mapping[tp.name] = Type::error();
                        lit.type_args.push_back(Type::error());
                    } else {
                        lit.type_args.push_back(mit->second);
                    }
                }
            } else {
                bind_type_args(st.type_params, lit.type_args, offset, mapping);
            }
        } else if (!lit.type_args.empty()) {
            error(offset, "type '" + lit.name + "' does not take type arguments");
        }

        for (std::size_t fi = 0; fi < lit.fields.size(); ++fi) {
            if (field_index[fi] >= st.fields.size()) {
                continue;
            }
            expect_expr(*lit.fields[fi].value, subst_type(st.fields[field_index[fi]].second.ty, mapping),
                        lit.fields[fi].value->offset, "field");
        }

        Type result = Type::named(lit.name);
        result.args = lit.type_args;
        return result;
    }

Type TypeChecker::check_method_call(HirMethodCall& call, std::size_t offset) {
        Type recv_ty;
        bool associated = false;
        if (auto* var = std::get_if<HirVar>(&call.receiver->kind)) {
            const auto type_key = lookup_named(var->name);
            if (!lookup(var->name) && known_named_type(var->name)) {
                associated = true;
                recv_ty = Type::named(type_key);
                call.receiver->ty = recv_ty;
            }
        }
        if (!associated) {
            recv_ty = check_expr(*call.receiver);
        }
        call.associated = associated;

        if (call.null_safe) {
            if (associated) {
                error(offset, "cannot use '?.' on an associated function");
                for (auto& arg : call.args) {
                    check_expr(*arg);
                }
                return Type::error();
            }
            if (recv_ty.kind != TypeKind::Nullable) {
                if (recv_ty != Type::error()) {
                    error(offset, "'?.' requires a '" + type_name(recv_ty) + "?' value");
                }
                for (auto& arg : call.args) {
                    check_expr(*arg);
                }
                return Type::error();
            }
            recv_ty = recv_ty.elem();
        }

        if (recv_ty.kind == TypeKind::Dyn) {
            if (associated) {
                error(offset, "cannot call associated function on 'dyn " + recv_ty.name + "'");
                for (auto& arg : call.args) {
                    check_expr(*arg);
                }
                return Type::error();
            }
            auto trait_it = trait_methods_.find(recv_ty.name);
            if (trait_it == trait_methods_.end()) {
                error(offset, "unknown method '" + call.method + "' on 'dyn " + recv_ty.name + "'");
                for (auto& arg : call.args) {
                    check_expr(*arg);
                }
                return Type::error();
            }
            auto method_it = trait_it->second.find(call.method);
            if (method_it == trait_it->second.end()) {
                error(offset, "unknown method '" + call.method + "' on 'dyn " + recv_ty.name + "'");
                for (auto& arg : call.args) {
                    check_expr(*arg);
                }
                return Type::error();
            }
            const MethodSig& sig = method_it->second;
            if (sig.self_kind == SelfKind::Mut) {
                error(offset, "cannot call '" + call.method + "' on 'dyn " + recv_ty.name +
                                  "'; dyn only dispatches 'self' methods");
                for (auto& arg : call.args) {
                    check_expr(*arg);
                }
                return Type::error();
            }
            if (sig.self_kind == SelfKind::None) {
                error(offset, "cannot call associated function '" + call.method + "' on 'dyn " +
                                  recv_ty.name + "'");
                for (auto& arg : call.args) {
                    check_expr(*arg);
                }
                return Type::error();
            }
            for (auto& arg : call.args) {
                check_expr(*arg);
            }
            if (call.args.size() != sig.params.size()) {
                error(offset, "method '" + call.method + "' expects " + std::to_string(sig.params.size()) +
                                  " argument(s), got " + std::to_string(call.args.size()));
            }
            const std::size_t n = std::min(call.args.size(), sig.params.size());
            for (std::size_t i = 0; i < n; ++i) {
                expect_expr(*call.args[i], sig.params[i], call.args[i]->offset, "argument");
            }
            if (call.null_safe) {
                call.wrap_ret = sig.ret.kind != TypeKind::Nullable;
                return as_nullable(sig.ret);
            }
            return sig.ret;
        }

        if (recv_ty.kind != TypeKind::Named) {
            if (recv_ty != Type::error()) {
                error(offset, "method call requires a struct, found '" + type_name(recv_ty) + "'");
            }
            for (auto& arg : call.args) {
                check_expr(*arg);
            }
            return Type::error();
        }

        auto type_it = methods_.find(lookup_named(recv_ty.name));
        if (type_it == methods_.end()) {
            type_it = methods_.find(recv_ty.name);
        }
        if (type_it == methods_.end()) {
            error(offset, "unknown method '" + call.method + "' on '" + recv_ty.name + "'");
            for (auto& arg : call.args) {
                check_expr(*arg);
            }
            return Type::error();
        }

        auto method_it = type_it->second.find(call.method);
        if (method_it == type_it->second.end()) {
            error(offset, "unknown method '" + call.method + "' on '" + recv_ty.name + "'");
            for (auto& arg : call.args) {
                check_expr(*arg);
            }
            return Type::error();
        }

        for (auto& arg : call.args) {
            check_expr(*arg);
        }

        std::unordered_map<std::string, Type> mapping;
        if (const StructInfo* st = struct_of(recv_ty)) {
            mapping = struct_subst(*st, recv_ty);
        }
        const MethodSig* chosen =
            resolve_method_overload(method_it->second, call.method, call, offset, mapping);
        if (!chosen) {
            return Type::error();
        }
        for (std::size_t i = 0; i < call.args.size(); ++i) {
            expect_expr(*call.args[i], subst_type(chosen->params[i], mapping), call.args[i]->offset, "argument");
            if (i < chosen->param_mut.size() && chosen->param_mut[i] && !is_mut_place(*call.args[i])) {
                error(call.args[i]->offset, "mut argument must be a mutable place");
            }
        }
        if (!chosen->type_params.empty()) {
            check_type_arg_bounds(chosen->type_params, call.type_args, offset);
        }
        Type ret = subst_type(chosen->ret, mapping);
        if (call.null_safe) {
            call.wrap_ret = ret.kind != TypeKind::Nullable;
            return as_nullable(std::move(ret));
        }
        return ret;
    }

Type TypeChecker::check_field_assign(HirFieldAssign& as, std::size_t offset) {
        const Type base_ty = check_expr(*as.base);
        const Type value_ty = check_expr(*as.value);
        if (base_ty.kind == TypeKind::Tuple) {
            const auto idx = tuple_field_index(as.field);
            if (!idx || *idx >= base_ty.args.size()) {
                error(offset, "unknown field '" + as.field + "' on '" + type_name(base_ty) + "'");
                return Type::error();
            }
            if (!is_mut_place(*as.base)) {
                error(offset, "cannot assign to field '" + as.field + "' through an immutable value");
            }
            expect_expr(*as.value, base_ty.args[*idx], as.value->offset, "assignment");
            return base_ty.args[*idx];
        }
        const StructInfo* st = struct_of(base_ty);
        if (!st) {
            if (base_ty != Type::error()) {
                error(offset, "field assignment requires a struct, found '" + type_name(base_ty) + "'");
            }
            return Type::error();
        }
        if (st->opaque) {
            error(offset, "cannot assign fields of opaque type '" + type_name(base_ty) + "'");
            return Type::error();
        }

        const FieldInfo* info = find_field(*st, as.field);
        if (!info) {
            error(offset, "unknown field '" + as.field + "' on '" + type_name(base_ty) + "'");
            return Type::error();
        }
        if (!info->mut) {
            error(offset, "cannot assign to immutable field '" + as.field + "'");
        }
        if (!is_mut_place(*as.base)) {
            error(offset, "cannot assign to field '" + as.field + "' through an immutable value");
        }

        expect_expr(*as.value, subst_type(info->ty, struct_subst(*st, base_ty)), as.value->offset, "assignment");
        return subst_type(info->ty, struct_subst(*st, base_ty));
    }

Type TypeChecker::check_index(HirIndex& idx, std::size_t offset) {
        const Type base_ty = check_expr(*idx.base);
        check_expr(*idx.index);
        if (base_ty.kind == TypeKind::List || base_ty.kind == TypeKind::Array) {
            expect_expr(*idx.index, Type::i32(), idx.index->offset, "index");
            return base_ty.elem();
        }
        if (base_ty.kind == TypeKind::Dict) {
            expect_expr(*idx.index, base_ty.key(), idx.index->offset, "key");
            return base_ty.value();
        }
        if (base_ty != Type::error()) {
            error(offset, "cannot index '" + type_name(base_ty) + "'");
        }
        return Type::error();
    }

Type TypeChecker::check_index_assign(HirIndexAssign& as, std::size_t offset) {
        HirIndex idx{std::move(as.base), std::move(as.index)};
        const Type elem_ty = check_index(idx, offset);
        as.base = std::move(idx.base);
        as.index = std::move(idx.index);
        if (!is_mut_place(*as.base)) {
            error(offset, "cannot assign through an immutable collection");
        }
        expect_expr(*as.value, elem_ty, as.value->offset, "assignment");
        return elem_ty;
    }

Type TypeChecker::check_list_lit(HirListLit& lit, std::size_t offset) {
        if (lit.elems.empty()) {
            return Type::unknown();
        }
        Type elem = check_expr(*lit.elems.front());
        for (std::size_t i = 1; i < lit.elems.size(); ++i) {
            expect_expr(*lit.elems[i], elem, lit.elems[i]->offset, "list element");
        }
        if (lit.array) {
            return Type::array(elem, lit.elems.size());
        }
        return Type::list(elem);
    }

Type TypeChecker::check_tuple_lit(HirTupleLit& lit, std::size_t offset) {
        if (lit.elems.size() < 2) {
            error(offset, "tuple literal needs at least two elements");
            return Type::error();
        }
        std::vector<Type> args;
        args.reserve(lit.elems.size());
        for (auto& elem : lit.elems) {
            args.push_back(check_expr(*elem));
        }
        return Type::tuple(std::move(args));
    }

Type TypeChecker::check_dict_lit(HirDictLit& lit, std::size_t offset) {
        if (lit.entries.empty()) {
            return Type::unknown();
        }
        Type key = check_expr(*lit.entries.front().first);
        Type value = check_expr(*lit.entries.front().second);
        for (std::size_t i = 1; i < lit.entries.size(); ++i) {
            expect_expr(*lit.entries[i].first, key, lit.entries[i].first->offset, "dict key");
            expect_expr(*lit.entries[i].second, value, lit.entries[i].second->offset, "dict value");
        }
        return Type::dict(key, value);
    }

Type TypeChecker::check_enum_lit(HirEnumLit& lit, std::size_t offset) {
        lit.enum_name = lookup_named(lit.enum_name);
        auto c_it = c_enums_.find(lit.enum_name);
        if (c_it != c_enums_.end()) {
            if (lit.tuple || !lit.fields.empty() || !lit.args.empty()) {
                error(offset, "enum member '" + lit.variant + "' cannot have fields");
                return Type::named(lit.enum_name);
            }
            if (!find_member(c_it->second, lit.variant)) {
                error(offset, "unknown member '" + lit.variant + "' on '" + lit.enum_name + "'");
                return Type::error();
            }
            return Type::named(lit.enum_name);
        }

        auto it = variants_.find(lit.enum_name);
        if (it == variants_.end()) {
            error(offset, "unknown type '" + lit.enum_name + "'");
            return Type::error();
        }
        const VariantInfo* v = find_variant(it->second, lit.variant);
        if (!v) {
            error(offset, "unknown variant '" + lit.variant + "' on '" + lit.enum_name + "'");
            return Type::error();
        }
        if (lit.tuple != v->tuple) {
            error(offset, "variant '" + lit.variant + "' has a different shape");
            return Type::named(lit.enum_name);
        }
        if (lit.tuple) {
            if (lit.args.size() != v->fields.size()) {
                error(offset, "variant '" + lit.variant + "' expects " +
                                  std::to_string(v->fields.size()) + " argument(s)");
            }
            const std::size_t n = std::min(lit.args.size(), v->fields.size());
            for (std::size_t i = 0; i < lit.args.size(); ++i) {
                check_expr(*lit.args[i]);
                if (i < n) {
                    expect_expr(*lit.args[i], v->fields[i].second.ty, lit.args[i]->offset, "argument");
                }
            }
        } else {
            std::vector<bool> seen(v->fields.size(), false);
            for (auto& field : lit.fields) {
                check_expr(*field.value);
                std::size_t index = v->fields.size();
                for (std::size_t i = 0; i < v->fields.size(); ++i) {
                    if (v->fields[i].first == field.name) {
                        index = i;
                        break;
                    }
                }
                if (index == v->fields.size()) {
                    error(field.value->offset,
                          "unknown field '" + field.name + "' on '" + lit.variant + "'");
                    continue;
                }
                seen[index] = true;
                expect_expr(*field.value, v->fields[index].second.ty, field.value->offset, "field");
            }
            for (std::size_t i = 0; i < v->fields.size(); ++i) {
                if (!seen[i]) {
                    error(offset, "missing field '" + v->fields[i].first + "' in '" + lit.variant + "'");
                }
            }
        }
        return Type::named(lit.enum_name);
    }

Type TypeChecker::check_match(HirMatch& match, std::size_t offset) {
        const Type scrut = check_expr(*match.scrutinee);
        if (match.arms.empty()) {
            error(offset, "match needs at least one arm");
            return Type::error();
        }

        Type result;
        bool first = true;
        std::vector<bool> covered;
        const EnumInfo* adt = nullptr;
        const CEnumInfo* cen = nullptr;
        if (scrut.kind == TypeKind::Named) {
            auto v_it = variants_.find(lookup_named(scrut.name));
            if (v_it != variants_.end()) {
                adt = &v_it->second;
                covered.assign(adt->variants.size(), false);
            } else {
                auto c_it = c_enums_.find(lookup_named(scrut.name));
                if (c_it != c_enums_.end()) {
                    cen = &c_it->second;
                    covered.assign(cen->members.size(), false);
                }
            }
        }

        for (auto& arm : match.arms) {
            push_scope();
            check_pat(*arm.pat, scrut, adt, cen, covered);
            check_expr(*arm.body);
            if (first) {
                result = arm.body->ty;
                first = false;
            } else {
                expect_expr(*arm.body, result, arm.body->offset, "match arm");
            }
            pop_scope();
        }

        if (adt) {
            for (std::size_t i = 0; i < covered.size(); ++i) {
                if (!covered[i]) {
                    error(offset, "non-exhaustive match, missing '" + adt->variants[i].first + "'");
                }
            }
        }
        if (cen) {
            for (std::size_t i = 0; i < covered.size(); ++i) {
                if (!covered[i]) {
                    error(offset, "non-exhaustive match, missing '" + cen->members[i].first + "'");
                }
            }
        }
        return result;
    }

void TypeChecker::check_pat(HirPat& pat, const Type& scrut, const EnumInfo* en, const CEnumInfo* cen,
 std::vector<bool>& covered) {
        std::string unit_variant;
        std::visit(
            [&](auto&& kind) {
                using K = std::decay_t<decltype(kind)>;
                if constexpr (std::is_same_v<K, HirPatWild>) {
                    for (std::size_t i = 0; i < covered.size(); ++i) {
                        covered[i] = true;
                    }
                } else if constexpr (std::is_same_v<K, HirPatBinding>) {
                    if (en) {
                        const VariantInfo* v = find_variant(*en, kind.name);
                        if (v && !lookup(kind.name)) {
                            covered[v->index] = true;
                            unit_variant = kind.name;
                            return;
                        }
                    }
                    if (cen) {
                        const CEnumMemberInfo* m = find_member(*cen, kind.name);
                        if (m && !lookup(kind.name)) {
                            covered[m->index] = true;
                            unit_variant = kind.name;
                            return;
                        }
                    }
                    declare(kind.name, Binding{scrut, false}, pat.offset);
                } else if constexpr (std::is_same_v<K, HirPatVariant>) {
                    if (cen) {
                        if (!kind.enum_name.empty() && kind.enum_name != scrut.name) {
                            error(pat.offset, "pattern has type '" + kind.enum_name + "', expected '" +
                                                  type_name(scrut) + "'");
                        }
                        const CEnumMemberInfo* m = find_member(*cen, kind.variant);
                        if (!m) {
                            error(pat.offset, "unknown member '" + kind.variant + "'");
                            return;
                        }
                        if (kind.tuple || !kind.fields.empty() || !kind.args.empty()) {
                            error(pat.offset, "enum member '" + kind.variant + "' cannot have fields");
                        }
                        covered[m->index] = true;
                        return;
                    }
                    const EnumInfo* used = en;
                    if (!kind.enum_name.empty()) {
                        auto it = variants_.find(lookup_named(kind.enum_name));
                        if (it == variants_.end()) {
                            error(pat.offset, "unknown variant type '" + kind.enum_name + "'");
                            return;
                        }
                        used = &it->second;
                        if (scrut.kind == TypeKind::Named && lookup_named(scrut.name) != it->first) {
                            error(pat.offset, "pattern has type '" + kind.enum_name + "', expected '" +
                                                  type_name(scrut) + "'");
                        }
                    }
                    if (!used) {
                        error(pat.offset, "variant pattern requires a variant type");
                        return;
                    }
                    const VariantInfo* v = find_variant(*used, kind.variant);
                    if (!v) {
                        error(pat.offset, "unknown variant '" + kind.variant + "'");
                        return;
                    }
                    if (v->index < covered.size()) {
                        covered[v->index] = true;
                    }
                    if (kind.tuple) {
                        if (kind.args.size() != v->fields.size()) {
                            error(pat.offset, "variant '" + kind.variant + "' expects " +
                                                  std::to_string(v->fields.size()) + " binding(s)");
                        }
                        const std::size_t n = std::min(kind.args.size(), v->fields.size());
                        for (std::size_t i = 0; i < n; ++i) {
                            if (auto* bind = std::get_if<HirPatBinding>(&kind.args[i]->kind)) {
                                declare(bind->name, Binding{v->fields[i].second.ty, false},
                                        kind.args[i]->offset);
                            } else if (!std::holds_alternative<HirPatWild>(kind.args[i]->kind)) {
                                error(kind.args[i]->offset, "nested patterns are not supported yet");
                            }
                        }
                    } else {
                        for (const auto& fname : kind.fields) {
                            const FieldInfo* f = nullptr;
                            for (const auto& [name, info] : v->fields) {
                                if (name == fname) {
                                    f = &info;
                                    break;
                                }
                            }
                            if (!f) {
                                error(pat.offset, "unknown field '" + fname + "' in '" + kind.variant + "'");
                                continue;
                            }
                            declare(fname, Binding{f->ty, false}, pat.offset);
                        }
                    }
                }
            },
            pat.kind);
        if (!unit_variant.empty()) {
            pat.kind = HirPatVariant{scrut.name, std::move(unit_variant), false, {}, {}};
        }
    }

bool TypeChecker::coerce_collection(HirExpr& expr, const Type& expected) {
        if (auto* list = std::get_if<HirListLit>(&expr.kind)) {
            if (expected.kind == TypeKind::List) {
                if (list->elems.empty()) {
                    expr.ty = expected;
                    return true;
                }
                if (expr.ty.kind == TypeKind::List && expr.ty.elem() == expected.elem()) {
                    expr.ty = expected;
                    return true;
                }
            }
            if (expected.kind == TypeKind::Array) {
                if (list->elems.size() != expected.size) {
                    return false;
                }
                if (list->elems.empty() ||
                    (expr.ty.kind == TypeKind::List && expr.ty.elem() == expected.elem()) ||
                    (expr.ty.kind == TypeKind::Array && expr.ty.elem() == expected.elem())) {
                    list->array = true;
                    expr.ty = expected;
                    return true;
                }
            }
        }
        if (auto* dict = std::get_if<HirDictLit>(&expr.kind)) {
            if (expected.kind == TypeKind::Dict && dict->entries.empty()) {
                expr.ty = expected;
                return true;
            }
        }
        return false;
    }

void TypeChecker::expect_expr(HirExpr& expr, Type expected, std::size_t offset, const char* what) {
        const Type got = (expr.ty.kind == TypeKind::Unknown || expr.ty.kind == TypeKind::Error)
                             ? check_expr(expr)
                             : expr.ty;
        if (coerce_collection(expr, expected)) {
            return;
        }
        if (got.kind == TypeKind::Unknown) {
            if (auto* var = std::get_if<HirVar>(&expr.kind)) {
                if (bind_fn_value(*var, expr, &expected)) {
                    return;
                }
            }
        }
        if (got == expected || got == Type::error() || expected == Type::error() ||
            got.kind == TypeKind::Never) {
            return;
        }
        if (expected.kind == TypeKind::Nullable && got == expected.elem()) {
            expr.coerce_nullable = true;
            expr.ty = expected;
            return;
        }
        if (expected.kind == TypeKind::Dyn && got.kind == TypeKind::Named) {
            auto it = trait_impls_.find(lookup_named(got.name));
            if (it == trait_impls_.end()) {
                it = trait_impls_.find(got.name);
            }
            if (it != trait_impls_.end() && it->second.contains(expected.name)) {
                expr.coerce_dyn = expected.name;
                return;
            }
            error(offset, "type '" + type_name(got) + "' does not implement '" + expected.name + "'");
            return;
        }
        if (coerce_lit(expr, expected)) {
            return;
        }
        error(offset, std::string(what) + " has type '" + type_name(got) + "', expected '" +
                          type_name(expected) + "'");
    }

void TypeChecker::expect_type(Type got, Type expected, std::size_t offset, const char* what) {
        if (got == Type::error() || expected == Type::error() || got == expected) {
            return;
        }
        error(offset, std::string(what) + " has type '" + type_name(got) + "', expected '" +
                          type_name(expected) + "'");
    }

}  // namespace qpc::detail

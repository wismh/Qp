#include "compiler/typeck/type_checker.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace qpc::detail {

Binding* TypeChecker::lookup(const std::string& name) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            if (auto found = it->find(name); found != it->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }

bool TypeChecker::ends_with_return(const HirBlock& body) {
        return !body.stmts.empty() && std::holds_alternative<HirReturn>(body.stmts.back()->kind);
    }

Binding* TypeChecker::lookup_static(const std::string& name) {
        if (!current_prefix_.empty() && !name_has_path(name)) {
            if (auto it = statics_.find(qualify(current_prefix_, name)); it != statics_.end()) {
                return &it->second;
            }
        }
        if (auto it = statics_.find(name); it != statics_.end()) {
            return &it->second;
        }
        if (auto alias = static_aliases_.find(name); alias != static_aliases_.end()) {
            if (auto it = statics_.find(alias->second); it != statics_.end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

Binding* TypeChecker::lookup_binding(const std::string& name) {
        if (auto* b = lookup(name)) {
            return b;
        }
        return lookup_static(name);
    }

bool TypeChecker::is_generic_param(const std::vector<HirTypeParam>& tps, const std::string& name) {
        for (const auto& tp : tps) {
            if (tp.name == name) {
                return true;
            }
        }
        return false;
    }

Type TypeChecker::as_nullable(Type t) {
        if (t.kind == TypeKind::Nullable || t.kind == TypeKind::Error) {
            return t;
        }
        return Type::nullable(std::move(t));
    }

bool TypeChecker::same_param_types(const std::vector<Type>& a, const std::vector<Type>& b) {
        return a == b;
    }

bool TypeChecker::add_fn_sig(const std::string& key, FnSig sig, std::size_t offset,
                             const std::string& display_name) {
        auto& overloads = sigs_[key];
        if (sig.c_abi && !overloads.empty()) {
            error(offset, "cannot overload extern \"C\" function '" + display_name + "'");
            return false;
        }
        for (const auto& existing : overloads) {
            if (existing.c_abi) {
                error(offset, "cannot overload extern \"C\" function '" + display_name + "'");
                return false;
            }
            if (same_param_types(existing.params, sig.params)) {
                error(offset, "duplicate function '" + display_name + "'");
                return false;
            }
        }
        overloads.push_back(std::move(sig));
        return true;
    }

bool TypeChecker::add_method_sig(std::unordered_map<std::string, std::vector<MethodSig>>& table,
                                 const std::string& name, MethodSig sig, std::size_t offset) {
        auto& overloads = table[name];
        for (const auto& existing : overloads) {
            if (existing.self_kind == sig.self_kind && same_param_types(existing.params, sig.params)) {
                error(offset, "duplicate method '" + name + "'");
                return false;
            }
        }
        overloads.push_back(std::move(sig));
        return true;
    }

int TypeChecker::overload_arg_score(const HirExpr& expr, const Type& expected) const {
        const Type& got = expr.ty;
        if (got == Type::error() || expected == Type::error() || got.kind == TypeKind::Never) {
            return 0;
        }
        if (got == expected) {
            return 100;
        }
        if (auto* lit = std::get_if<HirLitInt>(&expr.kind)) {
            if (lit->unsuffixed && is_int(expected) && int_fits(lit->value, expected)) {
                return 50;
            }
        }
        if (auto* lit = std::get_if<HirLitFloat>(&expr.kind)) {
            if (lit->unsuffixed && is_float(expected)) {
                return 50;
            }
        }
        if (std::holds_alternative<HirLitNull>(expr.kind) && expected.kind == TypeKind::Nullable) {
            return 50;
        }
        if (expected.kind == TypeKind::Nullable && got == expected.elem()) {
            return 40;
        }
        if (expected.kind == TypeKind::Dyn && got.kind == TypeKind::Named) {
            auto it = trait_impls_.find(lookup_named(got.name));
            if (it == trait_impls_.end()) {
                it = trait_impls_.find(got.name);
            }
            if (it != trait_impls_.end() && it->second.contains(expected.name)) {
                return 40;
            }
        }
        if (const auto* list = std::get_if<HirListLit>(&expr.kind)) {
            if (expected.kind == TypeKind::List && list->elems.empty()) {
                return 40;
            }
            if (expected.kind == TypeKind::Array && list->elems.size() == expected.size &&
                (list->elems.empty() ||
                 (got.kind == TypeKind::List && got.elem() == expected.elem()) ||
                 (got.kind == TypeKind::Array && got.elem() == expected.elem()))) {
                return 40;
            }
        }
        if (const auto* dict = std::get_if<HirDictLit>(&expr.kind)) {
            if (expected.kind == TypeKind::Dict && dict->entries.empty()) {
                return 40;
            }
        }
        return -1;
    }

bool TypeChecker::try_score_overload(const std::vector<HirTypeParam>& type_params,
                                     const std::vector<Type>& params, std::vector<Type> type_args,
                                     const std::vector<HirExprPtr>& args, int& score,
                                     const std::unordered_map<std::string, Type>* base_mapping,
                                     std::unordered_map<std::string, Type>& mapping) {
        mapping = base_mapping ? *base_mapping : std::unordered_map<std::string, Type>{};
        if (params.size() != args.size()) {
            return false;
        }
        if (!type_args.empty()) {
            if (!type_arg_count_ok(type_params, type_args.size())) {
                return false;
            }
            const std::size_t prefix = pack_prefix(type_params);
            for (std::size_t i = 0; i < prefix; ++i) {
                mapping[type_params[i].name] = type_args[i];
            }
        } else if (last_is_pack(type_params)) {
            return false;
        } else if (!type_params.empty()) {
            for (std::size_t i = 0; i < params.size(); ++i) {
                if (!unify_type(params[i], args[i]->ty, type_params, mapping)) {
                    if (!(params[i].kind == TypeKind::Named && is_generic_param(type_params, params[i].name))) {
                        return false;
                    }
                    if (args[i]->ty.kind == TypeKind::Unknown || args[i]->ty.kind == TypeKind::Error) {
                        return false;
                    }
                    mapping[params[i].name] = args[i]->ty;
                }
            }
            for (const auto& tp : type_params) {
                if (tp.pack) {
                    continue;
                }
                auto it = mapping.find(tp.name);
                if (it == mapping.end() || it->second.kind == TypeKind::Unknown ||
                    it->second.kind == TypeKind::Error) {
                    return false;
                }
            }
        }

        score = type_params.empty() ? 1000 : 0;
        for (std::size_t i = 0; i < params.size(); ++i) {
            const int s = overload_arg_score(*args[i], subst_type(params[i], mapping));
            if (s < 0) {
                return false;
            }
            score += s;
        }
        return true;
    }

const FnSig* TypeChecker::resolve_fn_overload(const std::vector<FnSig>& candidates, const std::string& name,
                                              HirCall& call, std::size_t offset,
                                              std::unordered_map<std::string, Type>& mapping) {
        struct Cand {
            const FnSig* sig = nullptr;
            int score = 0;
            std::unordered_map<std::string, Type> mapping;
            std::vector<Type> type_args;
        };
        std::vector<Cand> viable;
        for (const auto& sig : candidates) {
            Cand c;
            c.sig = &sig;
            c.type_args = call.type_args;
            if (!try_score_overload(sig.type_params, sig.params, c.type_args, call.args, c.score, nullptr,
                                    c.mapping)) {
                continue;
            }
            if (c.type_args.empty() && !sig.type_params.empty()) {
                c.type_args.reserve(sig.type_params.size());
                for (const auto& tp : sig.type_params) {
                    if (tp.pack) {
                        continue;
                    }
                    c.type_args.push_back(c.mapping.at(tp.name));
                }
            }
            viable.push_back(std::move(c));
        }
        if (viable.empty()) {
            if (candidates.size() == 1) {
                const auto& sig = candidates.front();
                mapping.clear();
                bind_or_infer_type_args(sig.type_params, call.type_args, sig.params, call.args, offset, mapping);
                if (call.args.size() != sig.params.size()) {
                    error(offset, "function '" + name + "' expects " + std::to_string(sig.params.size()) +
                                      " argument(s), got " + std::to_string(call.args.size()));
                }
                const std::size_t n = std::min(call.args.size(), sig.params.size());
                for (std::size_t i = 0; i < n; ++i) {
                    expect_expr(*call.args[i], subst_type(sig.params[i], mapping), call.args[i]->offset,
                                "argument");
                }
                return nullptr;
            }
            error(offset, "no matching overload for '" + name + "'");
            return nullptr;
        }
        std::sort(viable.begin(), viable.end(),
                  [](const Cand& a, const Cand& b) { return a.score > b.score; });
        if (viable.size() > 1 && viable[0].score == viable[1].score) {
            error(offset, "ambiguous call to '" + name + "'");
            return nullptr;
        }
        mapping = std::move(viable[0].mapping);
        call.type_args = std::move(viable[0].type_args);
        return viable[0].sig;
    }

const MethodSig* TypeChecker::resolve_method_overload(const std::vector<MethodSig>& candidates,
                                                      const std::string& method, HirMethodCall& call,
                                                      std::size_t offset,
                                                      std::unordered_map<std::string, Type>& mapping) {
        struct Cand {
            const MethodSig* sig = nullptr;
            int score = 0;
            std::unordered_map<std::string, Type> mapping;
            std::vector<Type> type_args;
        };
        const auto base = mapping;
        std::vector<Cand> viable;
        for (const auto& sig : candidates) {
            if (call.associated && sig.self_kind != SelfKind::None) {
                continue;
            }
            if (!call.associated && sig.self_kind == SelfKind::None) {
                continue;
            }
            if (!call.associated && sig.self_kind == SelfKind::Mut && !is_mut_place(*call.receiver)) {
                continue;
            }
            Cand c;
            c.sig = &sig;
            c.type_args = call.type_args;
            if (!try_score_overload(sig.type_params, sig.params, c.type_args, call.args, c.score, &base,
                                    c.mapping)) {
                continue;
            }
            if (c.type_args.empty() && !sig.type_params.empty()) {
                c.type_args.reserve(sig.type_params.size());
                for (const auto& tp : sig.type_params) {
                    if (tp.pack) {
                        continue;
                    }
                    c.type_args.push_back(c.mapping.at(tp.name));
                }
            }
            if (sig.self_kind == SelfKind::Mut) {
                c.score += 10;
            }
            viable.push_back(std::move(c));
        }
        if (viable.empty()) {
            if (candidates.size() == 1) {
                const auto& sig = candidates.front();
                if (call.associated && sig.self_kind != SelfKind::None) {
                    error(offset, "cannot call instance method '" + method + "' on a type");
                    return nullptr;
                }
                if (!call.associated && sig.self_kind == SelfKind::None) {
                    error(offset, "cannot call associated function '" + method + "' on a value");
                    return nullptr;
                }
                if (!call.associated && sig.self_kind == SelfKind::Mut && !is_mut_place(*call.receiver)) {
                    error(offset, "cannot call '" + method + "' on an immutable receiver");
                    return nullptr;
                }
                mapping = base;
                bind_or_infer_type_args(sig.type_params, call.type_args, sig.params, call.args, offset, mapping);
                if (call.args.size() != sig.params.size()) {
                    error(offset, "method '" + method + "' expects " + std::to_string(sig.params.size()) +
                                      " argument(s), got " + std::to_string(call.args.size()));
                }
                const std::size_t n = std::min(call.args.size(), sig.params.size());
                for (std::size_t i = 0; i < n; ++i) {
                    expect_expr(*call.args[i], subst_type(sig.params[i], mapping), call.args[i]->offset,
                                "argument");
                }
                return nullptr;
            }
            error(offset, "no matching overload for '" + method + "'");
            return nullptr;
        }
        std::sort(viable.begin(), viable.end(),
                  [](const Cand& a, const Cand& b) { return a.score > b.score; });
        if (viable.size() > 1 && viable[0].score == viable[1].score) {
            error(offset, "ambiguous call to '" + method + "'");
            return nullptr;
        }
        mapping = std::move(viable[0].mapping);
        call.type_args = std::move(viable[0].type_args);
        return viable[0].sig;
    }

}  // namespace qpc::detail

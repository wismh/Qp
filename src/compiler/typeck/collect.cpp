#include "compiler/typeck/type_checker.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace qpc::detail {

void TypeChecker::run() {
        collect_names(mod_, "");
        collect_type_details(mod_, "");
        collect_statics(mod_, "");
        apply_uses(mod_, "");
        if (diags_.has_errors()) {
            return;
        }
        collect_sigs_tree(mod_, "");
        if (diags_.has_errors()) {
            return;
        }
        push_scope();
        check_tree(mod_, "");
        pop_scope();
    }

void TypeChecker::error(std::size_t offset, std::string message) {
        diags_.error(*current_src_, offset, std::move(message));
    }

std::string TypeChecker::qualify(const std::string& prefix, const std::string& name) {
        return prefix.empty() ? name : prefix + "::" + name;
    }

bool TypeChecker::name_has_path(const std::string& name) {
        return name.find("::") != std::string::npos;
    }

bool TypeChecker::is_registered_named(const std::string& name) const {
        return structs_.contains(name) || variants_.contains(name) || c_enums_.contains(name) ||
               traits_.contains(name);
    }

std::string TypeChecker::lookup_named(const std::string& name) const {
        if (!name_has_path(name) && !current_prefix_.empty()) {
            const auto q = qualify(current_prefix_, name);
            if (is_registered_named(q)) {
                return q;
            }
        }
        if (is_registered_named(name)) {
            return name;
        }
        if (auto it = type_aliases_.find(name); it != type_aliases_.end()) {
            return it->second;
        }
        return name;
    }

bool TypeChecker::known_named_type(const std::string& name) const {
        const auto key = lookup_named(name);
        return structs_.contains(key) || variants_.contains(key) || c_enums_.contains(key);
    }

bool TypeChecker::resolve_type(Type& ty, std::size_t offset) {
        if (ty.kind == TypeKind::List || ty.kind == TypeKind::Array || ty.kind == TypeKind::Nullable) {
            if (ty.args.empty()) {
                error(offset, "invalid collection type");
                ty = Type::error();
                return false;
            }
            return resolve_type(ty.args[0], offset);
        }
        if (ty.kind == TypeKind::Dict) {
            if (ty.args.size() < 2) {
                error(offset, "invalid dict type");
                ty = Type::error();
                return false;
            }
            const bool ok_key = resolve_type(ty.args[0], offset);
            const bool ok_val = resolve_type(ty.args[1], offset);
            return ok_key && ok_val;
        }
        if (ty.kind == TypeKind::Fn || ty.kind == TypeKind::Tuple) {
            if (ty.args.empty()) {
                error(offset, ty.kind == TypeKind::Fn ? "invalid fn type" : "invalid tuple type");
                ty = Type::error();
                return false;
            }
            bool ok = true;
            for (auto& arg : ty.args) {
                ok = resolve_type(arg, offset) && ok;
            }
            return ok;
        }
        if (ty.kind == TypeKind::Dyn) {
            const auto key = lookup_named(ty.name);
            if (!traits_.contains(key)) {
                error(offset, "unknown trait '" + ty.name + "'");
                ty = Type::error();
                return false;
            }
            ty.name = key;
            return true;
        }
        if (ty.kind != TypeKind::Named) {
            return ty != Type::error();
        }
        if (generic_params_.contains(ty.name)) {
            if (!ty.args.empty()) {
                error(offset, "type parameter '" + ty.name + "' cannot take type arguments");
                ty = Type::error();
                return false;
            }
            return true;
        }
        const auto key = lookup_named(ty.name);
        if (auto it = structs_.find(key); it != structs_.end()) {
            ty.name = it->second.opaque ? path_leaf(key) : key;
            const auto& tps = it->second.type_params;
            if (tps.empty()) {
                if (!ty.args.empty()) {
                    error(offset, "type '" + ty.name + "' does not take type arguments");
                    ty = Type::error();
                    return false;
                }
                return true;
            }
            if (ty.args.size() != tps.size()) {
                error(offset, "generic struct '" + ty.name + "' expects " + std::to_string(tps.size()) +
                                  " type argument(s), got " + std::to_string(ty.args.size()));
                ty = Type::error();
                return false;
            }
            bool ok = true;
            for (auto& arg : ty.args) {
                ok = resolve_type(arg, offset) && ok;
            }
            return ok;
        }
        if (!variants_.contains(key) && !c_enums_.contains(key)) {
            error(offset, "unknown type '" + ty.name + "'");
            ty = Type::error();
            return false;
        }
        ty.name = key;
        if (!ty.args.empty()) {
            error(offset, "type '" + ty.name + "' does not take type arguments");
            ty = Type::error();
            return false;
        }
        return true;
    }

const FieldInfo* TypeChecker::find_field(const StructInfo& st, const std::string& name) const {
        for (const auto& [fname, info] : st.fields) {
            if (fname == name) {
                return &info;
            }
        }
        return nullptr;
    }

std::unordered_map<std::string, Type> TypeChecker::struct_subst(const StructInfo& st, const Type& ty) const {
        std::unordered_map<std::string, Type> mapping;
        const std::size_t n = std::min(st.type_params.size(), ty.args.size());
        for (std::size_t i = 0; i < n; ++i) {
            mapping[st.type_params[i].name] = ty.args[i];
        }
        return mapping;
    }

const StructInfo* TypeChecker::struct_of(const Type& ty) const {
        if (ty.kind != TypeKind::Named) {
            return nullptr;
        }
        auto it = structs_.find(lookup_named(ty.name));
        return it == structs_.end() ? nullptr : &it->second;
    }

std::string TypeChecker::path_leaf(const std::string& name) {
        const auto pos = name.rfind("::");
        return pos == std::string::npos ? name : name.substr(pos + 2);
    }

void TypeChecker::collect_names(HirModule& m, const std::string& prefix) {
        SrcGuard src_guard(current_src_, m.source);
        for (auto& tr : m.traits) {
            traits_.insert(qualify(prefix, tr.name));
        }
        for (auto& en : m.enums) {
            const auto name = qualify(prefix, en.name);
            if (c_enums_.contains(name) || variants_.contains(name) || structs_.contains(name)) {
                error(en.offset, "duplicate type '" + name + "'");
                continue;
            }
            c_enums_.emplace(name, CEnumInfo{{}, en.offset});
        }
        for (auto& var : m.variants) {
            const auto name = qualify(prefix, var.name);
            if (c_enums_.contains(name) || variants_.contains(name) || structs_.contains(name)) {
                error(var.offset, "duplicate type '" + name + "'");
                continue;
            }
            variants_.emplace(name, EnumInfo{{}, var.offset});
        }
        for (auto& st : m.structs) {
            // Opaque extern types keep a root ABI name (qplus::T) and a module path alias.
            const auto qname = qualify(prefix, st.name);
            const auto abi = st.opaque ? st.name : qname;
            if (structs_.contains(abi) || c_enums_.contains(abi) || variants_.contains(abi)) {
                error(st.offset, "duplicate type '" + abi + "'");
                continue;
            }
            if (st.opaque && !prefix.empty() &&
                (structs_.contains(qname) || c_enums_.contains(qname) || variants_.contains(qname))) {
                error(st.offset, "duplicate type '" + qname + "'");
                continue;
            }
            StructInfo info{{}, st.type_params, st.offset, st.opaque};
            structs_.emplace(abi, info);
            if (st.opaque && !prefix.empty()) {
                structs_.emplace(qname, info);
            }
        }
        for (auto& child : m.mods) {
            collect_names(child, qualify(prefix, child.name));
        }
    }

void TypeChecker::collect_type_details(HirModule& m, const std::string& prefix) {
        SrcGuard src_guard(current_src_, m.source);
        for (auto& child : m.mods) {
            collect_type_details(child, qualify(prefix, child.name));
        }

        PrefixGuard prefix_guard(current_prefix_, prefix);

        for (auto& en : m.enums) {
            auto it = c_enums_.find(qualify(prefix, en.name));
            if (it == c_enums_.end() || it->second.offset != en.offset) {
                continue;
            }
            CEnumInfo& info = it->second;
            for (std::size_t i = 0; i < en.members.size(); ++i) {
                auto& member = en.members[i];
                if (find_member(info, member.name)) {
                    error(member.offset, "duplicate enum member '" + member.name + "'");
                    continue;
                }
                info.members.emplace_back(member.name,
                                          CEnumMemberInfo{member.value, i, member.offset});
            }
        }

        for (auto& en : m.variants) {
            auto it = variants_.find(qualify(prefix, en.name));
            if (it == variants_.end() || it->second.offset != en.offset) {
                continue;
            }
            EnumInfo& info = it->second;
            for (std::size_t i = 0; i < en.variants.size(); ++i) {
                auto& variant = en.variants[i];
                if (find_variant(info, variant.name)) {
                    error(variant.offset, "duplicate variant '" + variant.name + "'");
                    continue;
                }
                VariantInfo vi;
                vi.tuple = variant.tuple;
                vi.index = i;
                vi.offset = variant.offset;
                for (auto& field : variant.fields) {
                    resolve_type(field.ty, field.offset);
                    vi.fields.emplace_back(field.name, FieldInfo{field.mut, field.ty, field.offset});
                }
                info.variants.emplace_back(variant.name, std::move(vi));
            }
        }

        for (auto& st : m.structs) {
            auto it = structs_.find(qualify(prefix, st.name));
            if (it == structs_.end() || it->second.offset != st.offset) {
                continue;
            }
            StructInfo& info = it->second;
            info.opaque = st.opaque;
            info.type_params = st.type_params;
            if (st.opaque) {
                continue;
            }
            generic_params_.clear();
            for (const auto& tp : st.type_params) {
                generic_params_.insert(tp.name);
            }
            for (auto& field : st.fields) {
                if (find_field(info, field.name)) {
                    error(field.offset, "duplicate field '" + field.name + "'");
                    continue;
                }
                resolve_type(field.ty, field.offset);
                info.fields.emplace_back(field.name, FieldInfo{field.mut, field.ty, field.offset});
            }
            generic_params_.clear();
        }
    }

void TypeChecker::apply_uses(HirModule& m, const std::string& prefix) {
        SrcGuard src_guard(current_src_, m.source);
        for (auto& u : m.uses) {
            if (u.path.empty() && !u.glob) {
                continue;
            }
            if (u.glob) {
                std::string head;
                for (std::size_t i = 0; i < u.path.size(); ++i) {
                    if (i != 0) {
                        head += "::";
                    }
                    head += u.path[i];
                }
                const std::string pfx = head + "::";
                auto alias_types = [&](const auto& map) {
                    for (const auto& [k, v] : map) {
                        (void)v;
                        if (k.rfind(pfx, 0) == 0) {
                            const auto short_name = k.substr(pfx.size());
                            if (!short_name.empty() && short_name.find("::") == std::string::npos) {
                                type_aliases_.emplace(short_name, k);
                            }
                        }
                    }
                };
                alias_types(structs_);
                alias_types(c_enums_);
                alias_types(variants_);
                auto alias_map = [&](auto& map) {
                    std::vector<std::pair<std::string, typename std::decay_t<decltype(map)>::mapped_type>>
                        extra;
                    for (auto& [k, v] : map) {
                        if (k.rfind(pfx, 0) == 0) {
                            extra.emplace_back(k.substr(pfx.size()), v);
                        }
                    }
                    for (auto& e : extra) {
                        if (e.first.find("::") == std::string::npos) {
                            map.emplace(e.first, e.second);
                        }
                    }
                };
                alias_map(sigs_);
                for (const auto& [k, v] : statics_) {
                    (void)v;
                    if (k.rfind(pfx, 0) == 0) {
                        const auto short_name = k.substr(pfx.size());
                        if (!short_name.empty() && short_name.find("::") == std::string::npos) {
                            static_aliases_.emplace(short_name, k);
                        }
                    }
                }
                continue;
            }
            std::string full;
            for (std::size_t i = 0; i < u.path.size(); ++i) {
                if (i != 0) {
                    full += "::";
                }
                full += u.path[i];
            }
            const std::string last = u.path.back();
            if (structs_.contains(full) || c_enums_.contains(full) || variants_.contains(full)) {
                type_aliases_.emplace(last, full);
            }
            if (auto it = sigs_.find(full); it != sigs_.end()) {
                sigs_.emplace(last, it->second);
            }
            if (auto it = methods_.find(full); it != methods_.end()) {
                methods_.emplace(last, it->second);
            }
            if (auto it = traits_.find(full); it != traits_.end()) {
                traits_.insert(last);
            }
            if (statics_.contains(full)) {
                static_aliases_.emplace(last, full);
            }
        }
        for (auto& child : m.mods) {
            apply_uses(child, qualify(prefix, child.name));
        }
    }

void TypeChecker::collect_sigs_tree(HirModule& m, const std::string& prefix) {
        SrcGuard src_guard(current_src_, m.source);
        PrefixGuard prefix_guard(current_prefix_, prefix);
        collect_sigs_of(m, prefix);
        collect_trait_methods_of(m, prefix);
        collect_methods_of(m, prefix);
        for (auto& st : m.statics) {
            if (st.ty.kind != TypeKind::Unknown) {
                resolve_type(st.ty, st.offset);
            }
        }
        for (auto& child : m.mods) {
            collect_sigs_tree(child, qualify(prefix, child.name));
        }
    }

Type TypeChecker::infer_static_init_type(const HirExpr& expr) {
        return std::visit(
            [&](auto&& kind) -> Type {
                using K = std::decay_t<decltype(kind)>;
                if constexpr (std::is_same_v<K, HirLitInt>) {
                    return kind.unsuffixed ? Type::i32() : kind.ty;
                } else if constexpr (std::is_same_v<K, HirLitFloat>) {
                    return kind.unsuffixed ? Type::f32() : kind.ty;
                } else if constexpr (std::is_same_v<K, HirLitBool>) {
                    return Type::boolean();
                } else if constexpr (std::is_same_v<K, HirLitChar>) {
                    return Type::char_();
                } else if constexpr (std::is_same_v<K, HirLitString>) {
                    return Type::string();
                } else {
                    return Type::unknown();
                }
            },
            expr.kind);
    }

void TypeChecker::collect_statics(HirModule& m, const std::string& prefix) {
        SrcGuard src_guard(current_src_, m.source);
        for (auto& child : m.mods) {
            collect_statics(child, qualify(prefix, child.name));
        }
        PrefixGuard prefix_guard(current_prefix_, prefix);
        for (auto& st : m.statics) {
            const auto qname = qualify(prefix, st.name);
            const auto name = st.is_extern ? st.name : qname;
            if (statics_.contains(name)) {
                error(st.offset, "duplicate static '" + name + "'");
                continue;
            }
            if (st.is_extern && !prefix.empty() && statics_.contains(qname)) {
                error(st.offset, "duplicate static '" + qname + "'");
                continue;
            }
            if (st.ty.kind != TypeKind::Unknown) {
                resolve_type(st.ty, st.offset);
            } else if (st.is_extern) {
                error(st.offset, "extern static '" + st.name + "' requires a type annotation");
                st.ty = Type::error();
            } else if (st.init) {
                st.ty = infer_static_init_type(*st.init);
                if (st.ty.kind == TypeKind::Unknown) {
                    error(st.offset, "cannot infer type of '" + st.name + "', add a type annotation");
                    st.ty = Type::error();
                }
            } else {
                error(st.offset, "cannot infer type of '" + st.name + "', add a type annotation");
                st.ty = Type::error();
            }
            statics_.emplace(name, Binding{st.ty, st.mut});
            if (st.is_extern && !prefix.empty()) {
                statics_.emplace(qname, Binding{st.ty, st.mut});
            }
        }
    }

void TypeChecker::check_tree(HirModule& m, const std::string& prefix) {
        SrcGuard src_guard(current_src_, m.source);
        PrefixGuard prefix_guard(current_prefix_, prefix);
        for (auto& st : m.statics) {
            if (st.ty.kind != TypeKind::Unknown) {
                resolve_type(st.ty, st.offset);
            }
            if (st.is_extern) {
                if (st.ty.kind == TypeKind::Unknown) {
                    error(st.offset, "extern static '" + st.name + "' requires a type annotation");
                    st.ty = Type::error();
                }
                declare(st.name, Binding{st.ty, st.mut}, st.offset);
                statics_[st.name] = Binding{st.ty, st.mut};
                if (!prefix.empty()) {
                    statics_[qualify(prefix, st.name)] = Binding{st.ty, st.mut};
                }
                continue;
            }
            const Type init_ty = check_expr(*st.init);
            if (st.ty.kind == TypeKind::Unknown) {
                if (init_ty.kind == TypeKind::Unknown) {
                    error(st.offset, "cannot infer type of '" + st.name + "', add a type annotation");
                    st.ty = Type::error();
                } else {
                    st.ty = init_ty;
                }
            } else {
                expect_expr(*st.init, st.ty, st.init->offset, "static initializer");
            }
            declare(st.name, Binding{st.ty, st.mut}, st.offset);
            statics_[qualify(prefix, st.name)] = Binding{st.ty, st.mut};
        }
        for (auto& fn : m.functions) {
            if (!fn.is_extern) {
                check_fn(fn);
            }
        }
        for (auto& impl : m.impls) {
            for (auto& method : impl.methods) {
                if (!method.is_extern) {
                    check_fn(method);
                }
            }
        }
        for (auto& child : m.mods) {
            check_tree(child, qualify(prefix, child.name));
        }
    }

void TypeChecker::collect_sigs_of(HirModule& m, const std::string& prefix) {
        for (auto& fn : m.functions) {
            const auto qname = qualify(prefix, fn.name);
            const bool root_abi = fn.is_extern || fn.c_abi;
            const auto name = root_abi ? fn.name : qname;
            generic_params_.clear();
            for (const auto& tp : fn.type_params) {
                generic_params_.insert(tp.name);
            }
            resolve_type(fn.return_ty, fn.offset);
            FnSig sig;
            sig.type_params = fn.type_params;
            sig.ret = fn.return_ty;
            sig.offset = fn.offset;
            sig.c_abi = fn.c_abi;
            sig.params.reserve(fn.params.size());
            for (auto& p : fn.params) {
                resolve_type(p.ty, p.offset);
                sig.params.push_back(p.ty);
            }
            if (fn.c_abi) {
                check_c_abi_type(fn.return_ty, fn.offset, fn.name);
                for (const auto& p : fn.params) {
                    check_c_abi_type(p.ty, p.offset, fn.name);
                }
            }
            generic_params_.clear();
            if (!add_fn_sig(name, sig, fn.offset, name)) {
                continue;
            }
            if (!prefix.empty()) {
                if (root_abi) {
                    add_fn_sig(qname, sig, fn.offset, qname);
                } else {
                    add_fn_sig(fn.name, std::move(sig), fn.offset, fn.name);
                }
            }
        }
    }

void TypeChecker::collect_trait_methods_of(HirModule& m, const std::string& prefix) {
        for (auto& tr : m.traits) {
            auto& table = trait_methods_[tr.name];
            if (!prefix.empty()) {
                trait_methods_[qualify(prefix, tr.name)];
            }
            for (auto& method : tr.methods) {
                if (table.contains(method.name)) {
                    error(tr.offset, "duplicate method '" + method.name + "' in trait '" + tr.name + "'");
                    continue;
                }
                resolve_type(method.return_ty, tr.offset);
                MethodSig sig;
                sig.self_kind = method.self_kind;
                sig.ret = method.return_ty;
                sig.offset = tr.offset;
                sig.params.reserve(method.params.size());
                for (auto& p : method.params) {
                    resolve_type(p.ty, p.offset);
                    sig.params.push_back(p.ty);
                }
                table.emplace(method.name, sig);
                if (!prefix.empty()) {
                    trait_methods_[qualify(prefix, tr.name)].emplace(method.name, std::move(sig));
                }
            }
        }
    }

void TypeChecker::collect_methods_of(HirModule& m, const std::string& prefix) {
        for (auto& impl : m.impls) {
            const auto type_name = qualify(prefix, impl.type_name);
            const auto resolved = lookup_named(impl.type_name);
            const bool known = structs_.contains(type_name) || structs_.contains(resolved) ||
                               variants_.contains(type_name) || variants_.contains(resolved) ||
                               c_enums_.contains(type_name) || c_enums_.contains(resolved);
            if (!known) {
                error(impl.offset, "impl for unknown type '" + impl.type_name + "'");
                continue;
            }
            const std::string method_key = structs_.contains(type_name) || variants_.contains(type_name) ||
                                                   c_enums_.contains(type_name)
                                               ? type_name
                                               : resolved;
            if (auto st = structs_.find(method_key); st != structs_.end()) {
                const auto& st_tps = st->second.type_params;
                if (st_tps.empty()) {
                    if (!impl.type_params.empty()) {
                        error(impl.offset, "type '" + impl.type_name + "' is not generic");
                    }
                } else if (impl.type_params.size() != st_tps.size()) {
                    error(impl.offset, "impl of generic struct '" + impl.type_name + "' expects " +
                                          std::to_string(st_tps.size()) + " type parameter(s), got " +
                                          std::to_string(impl.type_params.size()));
                } else {
                    for (std::size_t i = 0; i < st_tps.size(); ++i) {
                        if (impl.type_params[i].name != st_tps[i].name) {
                            error(impl.offset, "type parameter '" + impl.type_params[i].name +
                                                  "' does not match '" + st_tps[i].name + "'");
                        }
                    }
                }
            }
            if (impl.trait_name) {
                const auto& trait = *impl.trait_name;
                const auto trait_key = lookup_named(trait);
                if (!is_op_trait(trait) && !traits_.contains(trait_key) && !traits_.contains(trait)) {
                    error(impl.offset, "unknown trait '" + trait + "'");
                    continue;
                }
                trait_impls_[method_key].insert(trait);
                if (!prefix.empty()) {
                    trait_impls_[impl.type_name].insert(trait);
                }
                if (is_op_trait(trait) && !traits_.contains(trait) && !traits_.contains(trait_key)) {
                    if (impl.methods.size() != 1) {
                        error(impl.offset, "operator impl '" + trait + "' needs one method");
                        continue;
                    }
                    auto& method = impl.methods[0];
                    const char* expected = trait == "Neg" ? "neg" : binop_method_from_trait(trait);
                    if (method.name != expected) {
                        error(method.offset, "operator impl '" + trait + "' needs fn " + expected);
                    }
                    generic_params_.clear();
                    for (const auto& tp : impl.type_params) {
                        generic_params_.insert(tp.name);
                    }
                    for (const auto& tp : method.type_params) {
                        generic_params_.insert(tp.name);
                    }
                    resolve_type(method.return_ty, method.offset);
                    MethodSig sig;
                    sig.self_kind = method.self_kind;
                    sig.type_params = method.type_params;
                    sig.ret = method.return_ty;
                    sig.offset = method.offset;
                    for (auto& p : method.params) {
                        resolve_type(p.ty, p.offset);
                        sig.params.push_back(p.ty);
                    }
                    auto& table = op_impls_[method_key];
                    if (table.contains(trait)) {
                        error(impl.offset, "duplicate impl " + trait + " for '" + impl.type_name + "'");
                        continue;
                    }
                    generic_params_.clear();
                    table.emplace(trait, sig);
                    if (!prefix.empty()) {
                        op_impls_[impl.type_name].emplace(trait, std::move(sig));
                    }
                }
                if (is_op_trait(trait)) {
                    continue;
                }
                check_trait_impl(impl, trait);
            }

            auto& table = methods_[method_key];
            for (auto& method : impl.methods) {
                generic_params_.clear();
                for (const auto& tp : impl.type_params) {
                    generic_params_.insert(tp.name);
                }
                for (const auto& tp : method.type_params) {
                    generic_params_.insert(tp.name);
                }
                resolve_type(method.return_ty, method.offset);
                MethodSig sig;
                sig.self_kind = method.self_kind;
                sig.type_params = method.type_params;
                sig.ret = method.return_ty;
                sig.offset = method.offset;
                sig.params.reserve(method.params.size());
                for (auto& p : method.params) {
                    resolve_type(p.ty, p.offset);
                    sig.params.push_back(p.ty);
                }
                generic_params_.clear();
                if (!add_method_sig(table, method.name, sig, method.offset)) {
                    continue;
                }
                if (!prefix.empty()) {
                    add_method_sig(methods_[impl.type_name], method.name, std::move(sig), method.offset);
                }
            }
        }
    }

void TypeChecker::check_trait_impl(const HirImpl& impl, const std::string& trait) {
        auto tit = trait_methods_.find(trait);
        if (tit == trait_methods_.end()) {
            tit = trait_methods_.find(lookup_named(trait));
        }
        if (tit == trait_methods_.end()) {
            return;
        }
        const auto& required = tit->second;
        for (const auto& method : impl.methods) {
            auto mit = required.find(method.name);
            if (mit == required.end()) {
                error(method.offset, "method '" + method.name + "' is not a member of trait '" + trait + "'");
                continue;
            }
            if (method.self_kind != mit->second.self_kind) {
                error(method.offset, "method '" + method.name + "' does not match trait '" + trait + "'");
            }
            if (method.params.size() != mit->second.params.size()) {
                error(method.offset, "method '" + method.name + "' expects " +
                                         std::to_string(mit->second.params.size()) +
                                         " argument(s) in trait '" + trait + "'");
            }
        }
        for (const auto& [name, _] : required) {
            (void)_;
            bool found = false;
            for (const auto& method : impl.methods) {
                if (method.name == name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                error(impl.offset, "missing method '" + name + "' in impl " + trait + " for '" +
                                       impl.type_name + "'");
            }
        }
    }

const VariantInfo* TypeChecker::find_variant(const EnumInfo& en, const std::string& name) const {
        for (const auto& [vname, info] : en.variants) {
            if (vname == name) {
                return &info;
            }
        }
        return nullptr;
    }

const CEnumMemberInfo* TypeChecker::find_member(const CEnumInfo& en, const std::string& name) const {
        for (const auto& [mname, info] : en.members) {
            if (mname == name) {
                return &info;
            }
        }
        return nullptr;
    }

void TypeChecker::check_c_abi_type(const Type& ty, std::size_t offset, const std::string& fn_name) {
        if (ty == Type::error() || is_c_abi_type(ty)) {
            return;
        }
        error(offset, "extern \"C\" function '" + fn_name + "' cannot use type '" + type_name(ty) + "'");
    }

const char* TypeChecker::binop_method_from_trait(const std::string& trait) {
        if (trait == "Add") {
            return "add";
        }
        if (trait == "Sub") {
            return "sub";
        }
        if (trait == "Mul") {
            return "mul";
        }
        if (trait == "Div") {
            return "div";
        }
        if (trait == "Rem") {
            return "rem";
        }
        return "add";
    }

}  // namespace qpc::detail

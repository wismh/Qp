#include "compiler/typeck.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qpc {
namespace {

struct Binding {
    Type ty;
    bool mut = false;
};

struct FnSig {
    std::vector<HirTypeParam> type_params;
    std::vector<Type> params;
    Type ret = Type::unit();
    std::size_t offset = 0;
};

struct MethodSig {
    SelfKind self_kind = SelfKind::None;
    std::vector<HirTypeParam> type_params;
    std::vector<Type> params;
    Type ret = Type::unit();
    std::size_t offset = 0;
};

struct FieldInfo {
    bool mut = false;
    Type ty;
    std::size_t offset = 0;
};

struct StructInfo {
    std::vector<std::pair<std::string, FieldInfo>> fields;
    std::size_t offset = 0;
    bool opaque = false;
};

struct VariantInfo {
    std::vector<std::pair<std::string, FieldInfo>> fields;
    bool tuple = false;
    std::size_t index = 0;
    std::size_t offset = 0;
};

struct CEnumMemberInfo {
    std::int64_t value = 0;
    std::size_t index = 0;
    std::size_t offset = 0;
};

struct CEnumInfo {
    std::vector<std::pair<std::string, CEnumMemberInfo>> members;
    std::size_t offset = 0;
};

struct EnumInfo {
    std::vector<std::pair<std::string, VariantInfo>> variants;
    std::size_t offset = 0;
};

static const char* binop_trait(BinOp op) {
    switch (op) {
        case BinOp::Add:
            return "Add";
        case BinOp::Sub:
            return "Sub";
        case BinOp::Mul:
            return "Mul";
        case BinOp::Div:
            return "Div";
        case BinOp::Mod:
            return "Rem";
        case BinOp::Eq:
        case BinOp::Ne:
        case BinOp::Lt:
        case BinOp::Le:
        case BinOp::Gt:
        case BinOp::Ge:
        case BinOp::And:
        case BinOp::Or:
            return "";
    }
    return "Add";
}

static bool is_eq_op(BinOp op) { return op == BinOp::Eq || op == BinOp::Ne; }

static bool is_ord_op(BinOp op) {
    return op == BinOp::Lt || op == BinOp::Le || op == BinOp::Gt || op == BinOp::Ge;
}

static bool is_logic_op(BinOp op) { return op == BinOp::And || op == BinOp::Or; }

static bool is_op_trait(std::string_view name) {
    return name == "Add" || name == "Sub" || name == "Mul" || name == "Div" || name == "Rem" ||
           name == "Neg";
}

class TypeChecker {
public:
    TypeChecker(const Source& src, HirModule& mod, DiagnosticEngine& diags)
        : current_src_(&src), mod_(mod), diags_(diags) {}

    void run() {
        collect_tree(mod_, "");
        apply_uses(mod_, "");
        if (diags_.has_errors()) {
            return;
        }
        push_scope();
        check_tree(mod_);
        pop_scope();
    }

private:
    const Source* current_src_;
    HirModule& mod_;
    DiagnosticEngine& diags_;
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, CEnumInfo> c_enums_;
    std::unordered_map<std::string, EnumInfo> variants_;
    std::unordered_map<std::string, FnSig> sigs_;
    std::unordered_map<std::string, std::unordered_map<std::string, MethodSig>> methods_;
    std::unordered_map<std::string, std::unordered_map<std::string, MethodSig>> op_impls_;
    std::unordered_map<std::string, std::unordered_set<std::string>> trait_impls_;
    std::unordered_set<std::string> traits_;
    std::unordered_set<std::string> generic_params_;
    std::vector<std::unordered_map<std::string, Binding>> scopes_;
    Type current_ret_ = Type::unit();
    int loop_depth_ = 0;

    void error(std::size_t offset, std::string message) {
        diags_.error(*current_src_, offset, std::move(message));
    }

    struct SrcGuard {
        const Source*& slot;
        const Source* prev;
        SrcGuard(const Source*& s, const Source* next) : slot(s), prev(s) {
            if (next) {
                slot = next;
            }
        }
        ~SrcGuard() { slot = prev; }
    };

    bool resolve_type(Type& ty, std::size_t offset) {
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
        if (ty.kind == TypeKind::Fn) {
            if (ty.args.empty()) {
                error(offset, "invalid fn type");
                ty = Type::error();
                return false;
            }
            bool ok = true;
            for (auto& arg : ty.args) {
                ok = resolve_type(arg, offset) && ok;
            }
            return ok;
        }
        if (ty.kind != TypeKind::Named) {
            return ty != Type::error();
        }
        if (!structs_.contains(ty.name) && !variants_.contains(ty.name) && !c_enums_.contains(ty.name) &&
            !generic_params_.contains(ty.name)) {
            error(offset, "unknown type '" + ty.name + "'");
            ty = Type::error();
            return false;
        }
        return true;
    }

    const FieldInfo* find_field(const StructInfo& st, const std::string& name) const {
        for (const auto& [fname, info] : st.fields) {
            if (fname == name) {
                return &info;
            }
        }
        return nullptr;
    }

    const StructInfo* struct_of(const Type& ty) const {
        if (ty.kind != TypeKind::Named) {
            return nullptr;
        }
        auto it = structs_.find(ty.name);
        return it == structs_.end() ? nullptr : &it->second;
    }

    static std::string qualify(const std::string& prefix, const std::string& name) {
        return prefix.empty() ? name : prefix + "::" + name;
    }

    void collect_tree(HirModule& m, const std::string& prefix) {
        SrcGuard src_guard(current_src_, m.source);
        for (auto& tr : m.traits) {
            traits_.insert(qualify(prefix, tr.name));
            traits_.insert(tr.name);
        }
        for (auto& en : m.enums) {
            const auto name = qualify(prefix, en.name);
            if (c_enums_.contains(name) || variants_.contains(name) || structs_.contains(name)) {
                error(en.offset, "duplicate type '" + name + "'");
                continue;
            }
            c_enums_.emplace(name, CEnumInfo{{}, en.offset});
            if (!prefix.empty()) {
                c_enums_.emplace(en.name, CEnumInfo{{}, en.offset});
            }
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
            const auto name = qualify(prefix, st.name);
            if (structs_.contains(name) || c_enums_.contains(name) || variants_.contains(name)) {
                error(st.offset, "duplicate type '" + name + "'");
                continue;
            }
            structs_.emplace(name, StructInfo{{}, st.offset, st.opaque});
        }

        if (prefix.empty()) {
            collect_c_enums();
            collect_variants();
            collect_structs();
        } else {
            for (auto& st : m.structs) {
                auto it = structs_.find(qualify(prefix, st.name));
                if (it == structs_.end()) {
                    continue;
                }
                it->second.opaque = st.opaque;
                if (st.opaque) {
                    continue;
                }
                for (auto& field : st.fields) {
                    resolve_type(field.ty, field.offset);
                    it->second.fields.emplace_back(field.name, FieldInfo{field.mut, field.ty, field.offset});
                }
            }
        }

        collect_sigs_of(m, prefix);
        collect_methods_of(m, prefix);

        for (auto& st : m.statics) {
            if (st.ty.kind != TypeKind::Unknown) {
                resolve_type(st.ty, st.offset);
            }
        }

        for (auto& child : m.mods) {
            collect_tree(child, qualify(prefix, child.name));
        }
    }

    void apply_uses(HirModule& m, const std::string& prefix) {
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
                auto alias_map = [&](auto& map) {
                    std::vector<std::pair<std::string, typename std::decay_t<decltype(map)>::mapped_type>> extra;
                    for (auto& [k, v] : map) {
                        if (k.rfind(pfx, 0) == 0) {
                            extra.emplace_back(k.substr(pfx.size()), v);
                        }
                    }
                    for (auto& e : extra) {
                        map.emplace(e.first, e.second);
                    }
                };
                alias_map(structs_);
                alias_map(sigs_);
                alias_map(c_enums_);
                alias_map(variants_);
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
            if (auto it = structs_.find(full); it != structs_.end()) {
                structs_.emplace(last, it->second);
            }
            if (auto it = sigs_.find(full); it != sigs_.end()) {
                sigs_.emplace(last, it->second);
            }
            if (auto it = methods_.find(full); it != methods_.end()) {
                methods_.emplace(last, it->second);
            }
            if (auto it = c_enums_.find(full); it != c_enums_.end()) {
                c_enums_.emplace(last, it->second);
            }
            if (auto it = variants_.find(full); it != variants_.end()) {
                variants_.emplace(last, it->second);
            }
            if (auto it = traits_.find(full); it != traits_.end()) {
                traits_.insert(last);
            }
        }
        for (auto& child : m.mods) {
            apply_uses(child, qualify(prefix, child.name));
        }
    }

    void check_tree(HirModule& m) {
        SrcGuard src_guard(current_src_, m.source);
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
            check_tree(child);
        }
    }

    void collect_sigs_of(HirModule& m, const std::string& prefix) {
        for (auto& fn : m.functions) {
            const auto name = qualify(prefix, fn.name);
            if (sigs_.contains(name)) {
                error(fn.offset, "duplicate function '" + name + "'");
                continue;
            }
            generic_params_.clear();
            for (const auto& tp : fn.type_params) {
                generic_params_.insert(tp.name);
            }
            resolve_type(fn.return_ty, fn.offset);
            FnSig sig;
            sig.type_params = fn.type_params;
            sig.ret = fn.return_ty;
            sig.offset = fn.offset;
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
            sigs_.emplace(name, sig);
            if (!prefix.empty()) {
                sigs_.emplace(fn.name, std::move(sig));
            }
        }
    }

    void collect_methods_of(HirModule& m, const std::string& prefix) {
        for (auto& impl : m.impls) {
            const auto type_name = qualify(prefix, impl.type_name);
            const bool known = structs_.contains(type_name) || structs_.contains(impl.type_name) ||
                               variants_.contains(type_name) || c_enums_.contains(type_name);
            if (!known) {
                error(impl.offset, "impl for unknown type '" + impl.type_name + "'");
                continue;
            }
            if (impl.trait_name) {
                const auto& trait = *impl.trait_name;
                if (!is_op_trait(trait) && !traits_.contains(trait)) {
                    error(impl.offset, "unknown trait '" + trait + "'");
                    continue;
                }
                trait_impls_[impl.type_name].insert(trait);
                if (is_op_trait(trait) && !traits_.contains(trait)) {
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
                    auto& table = op_impls_[impl.type_name];
                    if (table.contains(trait)) {
                        error(impl.offset, "duplicate impl " + trait + " for '" + impl.type_name + "'");
                        continue;
                    }
                    generic_params_.clear();
                    table.emplace(trait, std::move(sig));
                }
                continue;
            }

            auto& table = methods_[impl.type_name];
            for (auto& method : impl.methods) {
                if (table.contains(method.name)) {
                    error(method.offset, "duplicate method '" + method.name + "'");
                    continue;
                }
                generic_params_.clear();
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
                table.emplace(method.name, std::move(sig));
            }
        }
    }

    void collect_structs() {
        for (auto& st : mod_.structs) {
            auto it = structs_.find(st.name);
            if (it == structs_.end() || it->second.offset != st.offset) {
                continue;
            }

            StructInfo& info = it->second;
            info.opaque = st.opaque;
            if (st.opaque) {
                continue;
            }
            for (auto& field : st.fields) {
                if (find_field(info, field.name)) {
                    error(field.offset, "duplicate field '" + field.name + "'");
                    continue;
                }
                resolve_type(field.ty, field.offset);
                info.fields.emplace_back(field.name, FieldInfo{field.mut, field.ty, field.offset});
            }
        }
    }

    void collect_type_names() {
        for (auto& en : mod_.enums) {
            if (c_enums_.contains(en.name) || variants_.contains(en.name) || structs_.contains(en.name)) {
                error(en.offset, "duplicate type '" + en.name + "'");
                continue;
            }
            c_enums_.emplace(en.name, CEnumInfo{{}, en.offset});
        }
        for (auto& var : mod_.variants) {
            if (c_enums_.contains(var.name) || variants_.contains(var.name) || structs_.contains(var.name)) {
                error(var.offset, "duplicate type '" + var.name + "'");
                continue;
            }
            variants_.emplace(var.name, EnumInfo{{}, var.offset});
        }
    }

    const VariantInfo* find_variant(const EnumInfo& en, const std::string& name) const {
        for (const auto& [vname, info] : en.variants) {
            if (vname == name) {
                return &info;
            }
        }
        return nullptr;
    }

    const CEnumMemberInfo* find_member(const CEnumInfo& en, const std::string& name) const {
        for (const auto& [mname, info] : en.members) {
            if (mname == name) {
                return &info;
            }
        }
        return nullptr;
    }

    void collect_c_enums() {
        for (auto& en : mod_.enums) {
            auto it = c_enums_.find(en.name);
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
    }

    void collect_variants() {
        for (auto& en : mod_.variants) {
            auto it = variants_.find(en.name);
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
    }

    void collect_sigs() {
        for (auto& fn : mod_.functions) {
            if (sigs_.contains(fn.name)) {
                error(fn.offset, "duplicate function '" + fn.name + "'");
                continue;
            }

            resolve_type(fn.return_ty, fn.offset);
            FnSig sig;
            sig.ret = fn.return_ty;
            sig.offset = fn.offset;
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
            sigs_.emplace(fn.name, std::move(sig));
        }
    }

    void check_c_abi_type(const Type& ty, std::size_t offset, const std::string& fn_name) {
        if (ty == Type::error() || is_c_abi_type(ty)) {
            return;
        }
        error(offset, "extern \"C\" function '" + fn_name + "' cannot use type '" + type_name(ty) + "'");
    }

    void collect_methods() {
        for (auto& impl : mod_.impls) {
            const bool known = structs_.contains(impl.type_name) || variants_.contains(impl.type_name) ||
                               c_enums_.contains(impl.type_name);
            if (!known) {
                error(impl.offset, "impl for unknown type '" + impl.type_name + "'");
                continue;
            }

            if (impl.trait_name) {
                if (!is_op_trait(*impl.trait_name)) {
                    error(impl.offset, "unknown operator trait '" + *impl.trait_name + "'");
                    continue;
                }
                if (impl.methods.size() != 1) {
                    error(impl.offset, "operator impl '" + *impl.trait_name + "' needs one method");
                    continue;
                }
                auto& method = impl.methods[0];
                const char* expected =
                    *impl.trait_name == "Neg" ? "neg" : binop_method_from_trait(*impl.trait_name);
                if (method.name != expected) {
                    error(method.offset, "operator impl '" + *impl.trait_name + "' needs fn " + expected);
                }
                resolve_type(method.return_ty, method.offset);
                MethodSig sig;
                sig.self_kind = method.self_kind;
                sig.ret = method.return_ty;
                sig.offset = method.offset;
                for (auto& p : method.params) {
                    resolve_type(p.ty, p.offset);
                    sig.params.push_back(p.ty);
                }
                auto& table = op_impls_[impl.type_name];
                if (table.contains(*impl.trait_name)) {
                    error(impl.offset, "duplicate impl " + *impl.trait_name + " for '" + impl.type_name + "'");
                    continue;
                }
                table.emplace(*impl.trait_name, std::move(sig));
                continue;
            }

            auto& table = methods_[impl.type_name];
            for (auto& method : impl.methods) {
                if (table.contains(method.name)) {
                    error(method.offset, "duplicate method '" + method.name + "'");
                    continue;
                }
                if (method.self_kind == SelfKind::None) {
                    error(method.offset, "method '" + method.name + "' needs self");
                }

                resolve_type(method.return_ty, method.offset);
                MethodSig sig;
                sig.self_kind = method.self_kind;
                sig.ret = method.return_ty;
                sig.offset = method.offset;
                sig.params.reserve(method.params.size());
                for (auto& p : method.params) {
                    resolve_type(p.ty, p.offset);
                    sig.params.push_back(p.ty);
                }
                table.emplace(method.name, std::move(sig));
            }
        }
    }

    static const char* binop_method_from_trait(const std::string& trait) {
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

    void push_scope() { scopes_.emplace_back(); }

    void pop_scope() { scopes_.pop_back(); }

    bool declare(const std::string& name, Binding binding, std::size_t offset) {
        auto& top = scopes_.back();
        if (top.contains(name)) {
            error(offset, "duplicate variable '" + name + "'");
            return false;
        }
        top.emplace(name, binding);
        return true;
    }

    Binding* lookup(const std::string& name) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            if (auto found = it->find(name); found != it->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }

    void check_fn(HirFn& fn) {
        current_ret_ = fn.return_ty;
        push_scope();
        generic_params_.clear();
        for (const auto& tp : fn.type_params) {
            generic_params_.insert(tp.name);
            if (tp.bound && !traits_.contains(*tp.bound) && !is_op_trait(*tp.bound)) {
                error(fn.offset, "unknown trait bound '" + *tp.bound + "'");
            }
        }

        if (fn.self_kind != SelfKind::None) {
            declare("self", Binding{Type::named(fn.self_ty), fn.self_kind == SelfKind::Mut}, fn.offset);
        }

        for (const auto& p : fn.params) {
            declare(p.name, Binding{p.ty, true}, p.offset);
        }

        for (auto& stmt : fn.body.stmts) {
            check_stmt(*stmt);
        }

        if (fn.body.tail) {
            expect_expr(*fn.body.tail, fn.return_ty, fn.body.tail->offset, "function body");
        } else if (fn.return_ty != Type::unit() && !ends_with_return(fn.body)) {
            error(fn.offset, "missing return value in function '" + fn.name + "'");
        }

        pop_scope();
        generic_params_.clear();
    }

    static bool ends_with_return(const HirBlock& body) {
        return !body.stmts.empty() && std::holds_alternative<HirReturn>(body.stmts.back()->kind);
    }

    void check_stmt(HirStmt& stmt) {
        std::visit(
            [&](auto&& kind) {
                using K = std::decay_t<decltype(kind)>;
                if constexpr (std::is_same_v<K, HirLet>) {
                    check_let(stmt.offset, kind);
                } else if constexpr (std::is_same_v<K, HirReturn>) {
                    check_return(stmt.offset, kind);
                } else if constexpr (std::is_same_v<K, HirExprStmt>) {
                    check_expr(*kind.expr);
                } else if constexpr (std::is_same_v<K, HirWhile>) {
                    expect_expr(*kind.cond, Type::boolean(), kind.cond->offset, "while condition");
                    ++loop_depth_;
                    push_scope();
                    for (auto& s : kind.stmts) {
                        check_stmt(*s);
                    }
                    if (kind.tail) {
                        check_expr(*kind.tail);
                    }
                    pop_scope();
                    --loop_depth_;
                } else if constexpr (std::is_same_v<K, HirFor>) {
                    const Type iter_ty = check_expr(*kind.iter);
                    Type elem = Type::error();
                    if (std::holds_alternative<HirRange>(kind.iter->kind)) {
                        elem = iter_ty;
                    } else if (iter_ty.kind == TypeKind::List || iter_ty.kind == TypeKind::Array) {
                        elem = iter_ty.elem();
                    } else if (iter_ty != Type::error()) {
                        error(stmt.offset, "for-loop requires a list, array or range");
                    }
                    ++loop_depth_;
                    push_scope();
                    declare(kind.name, Binding{elem, false}, stmt.offset);
                    for (auto& s : kind.stmts) {
                        check_stmt(*s);
                    }
                    if (kind.tail) {
                        check_expr(*kind.tail);
                    }
                    pop_scope();
                    --loop_depth_;
                } else if constexpr (std::is_same_v<K, HirBreak> || std::is_same_v<K, HirContinue>) {
                    if (loop_depth_ == 0) {
                        error(stmt.offset, "break/continue outside of a loop");
                    }
                }
            },
            stmt.kind);
    }

    void check_let(std::size_t offset, HirLet& let) {
        const Type init_ty = check_expr(*let.init);
        if (let.ty.kind == TypeKind::Unknown) {
            if (init_ty.kind == TypeKind::Unknown) {
                error(offset, "cannot infer type of '" + let.name + "', add a type annotation");
                let.ty = Type::error();
            } else {
                let.ty = init_ty;
            }
        } else {
            resolve_type(let.ty, offset);
            expect_expr(*let.init, let.ty, let.init->offset, "let initializer");
        }
        declare(let.name, Binding{let.ty, let.mut}, offset);
    }

    void check_return(std::size_t offset, HirReturn& ret) {
        if (ret.value) {
            if (current_ret_.kind == TypeKind::Unknown) {
                current_ret_ = check_expr(*ret.value);
                return;
            }
            expect_expr(*ret.value, current_ret_, ret.value->offset, "return value");
            return;
        }
        if (current_ret_.kind != TypeKind::Unknown && current_ret_ != Type::unit()) {
            error(offset, "missing return value");
        }
        if (current_ret_.kind == TypeKind::Unknown) {
            current_ret_ = Type::unit();
        }
    }

    Type check_expr(HirExpr& expr) {
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
                    ty = check_var(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirBinary>) {
                    ty = check_binop(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirUnary>) {
                    ty = check_unary(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirCall>) {
                    ty = check_call(kind, expr.offset);
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
                }
            },
            expr.kind);
        expr.ty = ty;
        return ty;
    }

    Type check_var(const HirVar& var, std::size_t offset) {
        if (auto* b = lookup(var.name)) {
            return b->ty;
        }
        error(offset, "unknown identifier '" + var.name + "'");
        return Type::error();
    }

    Type subst_type(Type t, const std::unordered_map<std::string, Type>& mapping) const {
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

    static bool is_generic_param(const std::vector<HirTypeParam>& tps, const std::string& name) {
        for (const auto& tp : tps) {
            if (tp.name == name) {
                return true;
            }
        }
        return false;
    }

    bool unify_type(const Type& pattern, const Type& actual, const std::vector<HirTypeParam>& tps,
                    std::unordered_map<std::string, Type>& mapping) {
        if (pattern.kind == TypeKind::Named && is_generic_param(tps, pattern.name)) {
            auto it = mapping.find(pattern.name);
            if (it == mapping.end()) {
                if (actual.kind == TypeKind::Unknown || actual.kind == TypeKind::Error) {
                    return false;
                }
                mapping[pattern.name] = actual;
                return true;
            }
            return it->second == actual;
        }
        if (pattern.kind != actual.kind || pattern.name != actual.name || pattern.size != actual.size ||
            pattern.args.size() != actual.args.size()) {
            return false;
        }
        for (std::size_t i = 0; i < pattern.args.size(); ++i) {
            if (!unify_type(pattern.args[i], actual.args[i], tps, mapping)) {
                return false;
            }
        }
        return true;
    }

    void check_type_arg_bounds(const std::vector<HirTypeParam>& tps, const std::vector<Type>& args,
                               std::size_t offset) {
        const std::size_t n = std::min(args.size(), tps.size());
        for (std::size_t i = 0; i < n; ++i) {
            if (!tps[i].bound) {
                continue;
            }
            const std::string& bound = *tps[i].bound;
            bool ok = false;
            if (args[i].kind == TypeKind::Named) {
                if (auto it = trait_impls_.find(args[i].name); it != trait_impls_.end()) {
                    ok = it->second.contains(bound);
                }
                if (!ok) {
                    if (auto it = op_impls_.find(args[i].name); it != op_impls_.end()) {
                        ok = it->second.contains(bound);
                    }
                }
            }
            if (!ok) {
                error(offset, "type '" + type_name(args[i]) + "' does not implement '" + bound + "'");
            }
        }
    }

    void bind_type_args(const std::vector<HirTypeParam>& tps, std::vector<Type>& args, std::size_t offset,
                        std::unordered_map<std::string, Type>& mapping) {
        if (args.size() != tps.size()) {
            error(offset, "expects " + std::to_string(tps.size()) + " type argument(s), got " +
                              std::to_string(args.size()));
        }
        const std::size_t n = std::min(args.size(), tps.size());
        for (std::size_t i = 0; i < n; ++i) {
            resolve_type(args[i], offset);
            mapping[tps[i].name] = args[i];
        }
        check_type_arg_bounds(tps, args, offset);
    }

    void infer_type_args(const std::vector<HirTypeParam>& tps, std::vector<Type>& type_args,
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

    void bind_or_infer_type_args(const std::vector<HirTypeParam>& tps, std::vector<Type>& type_args,
                                 const std::vector<Type>& params, std::vector<HirExprPtr>& args,
                                 std::size_t offset, std::unordered_map<std::string, Type>& mapping) {
        if (type_args.empty() && !tps.empty()) {
            infer_type_args(tps, type_args, params, args, offset, mapping);
            return;
        }
        bind_type_args(tps, type_args, offset, mapping);
    }

    Type check_if(HirIf& iff, std::size_t offset) {
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
            }
        }
        if (then_ty != else_ty) {
            error(offset, "if branches have types '" + type_name(then_ty) + "' and '" + type_name(else_ty) +
                              "'");
            return Type::error();
        }
        return then_ty;
    }

    Type check_range(HirRange& range, std::size_t offset) {
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

    Type check_closure(HirClosure& clo, std::size_t offset) {
        const Type saved_ret = current_ret_;
        push_scope();
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
        current_ret_ = saved_ret;
        return Type::fn(std::move(params), std::move(ret));
    }

    bool can_cast(const Type& from, const Type& to) const {
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

    Type check_cast(HirCast& c, std::size_t offset) {
        const Type from = check_expr(*c.expr);
        if (!can_cast(from, c.ty)) {
            error(offset, "cannot cast '" + type_name(from) + "' as '" + type_name(c.ty) + "'");
            return Type::error();
        }
        return c.ty;
    }

    Type check_unwrap(HirUnwrap& un, std::size_t offset) {
        const Type inner = check_expr(*un.expr);
        if (inner.kind == TypeKind::Nullable) {
            return inner.elem();
        }
        if (inner != Type::error()) {
            error(offset, "unwrap '!' requires a '" + type_name(inner) + "?' value");
        }
        return Type::error();
    }

    Type check_new(HirNew& n, std::size_t offset) {
        HirStructLit lit;
        lit.name = n.name;
        lit.fields = std::move(n.fields);
        const Type inner = check_struct_lit(lit, offset);
        n.fields = std::move(lit.fields);
        if (inner.kind == TypeKind::Error) {
            return Type::error();
        }
        return Type::nullable(inner);
    }

    Type check_unary(HirUnary& un, std::size_t offset) {
        const Type inner = check_expr(*un.operand);
        if (un.op == UnOp::Not) {
            expect_type(inner, Type::boolean(), offset, "operand");
            return Type::boolean();
        }
        if (is_signed_int(inner) || is_float(inner)) {
            return inner;
        }
        if (inner.kind == TypeKind::Named) {
            auto type_it = op_impls_.find(inner.name);
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

    bool coerce_lit(HirExpr& expr, Type expected) {
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

    Type check_binop(HirBinary& bin, std::size_t offset) {
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
            auto type_it = op_impls_.find(lhs.name);
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

    Type check_fn_value_call(const Type& fn_ty, HirCall& call, std::size_t offset, std::string callee) {
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

    Type check_call(HirCall& call, std::size_t offset) {
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
            error(offset, "unknown function '" + call.callee + "'");
            for (auto& arg : call.args) {
                check_expr(*arg);
            }
            return Type::error();
        }

        const FnSig& sig = it->second;
        for (auto& arg : call.args) {
            check_expr(*arg);
        }

        std::unordered_map<std::string, Type> mapping;
        bind_or_infer_type_args(sig.type_params, call.type_args, sig.params, call.args, offset, mapping);

        if (call.args.size() != sig.params.size()) {
            error(offset, "function '" + call.callee + "' expects " + std::to_string(sig.params.size()) +
                              " argument(s), got " + std::to_string(call.args.size()));
        }

        const std::size_t n = std::min(call.args.size(), sig.params.size());
        for (std::size_t i = 0; i < n; ++i) {
            expect_expr(*call.args[i], subst_type(sig.params[i], mapping), call.args[i]->offset, "argument");
        }
        return subst_type(sig.ret, mapping);
    }

    Type check_assign(HirAssign& as, std::size_t offset) {
        Binding* b = lookup(as.name);
        const Type value_ty = check_expr(*as.value);

        if (!b) {
            error(offset, "unknown identifier '" + as.name + "'");
            return Type::error();
        }
        if (!b->mut) {
            error(offset, "cannot assign to immutable variable '" + as.name + "'");
        }

        expect_expr(*as.value, b->ty, as.value->offset, "assignment");
        return b->ty;
    }

    bool is_mut_place(const HirExpr& expr) {
        if (const auto* var = std::get_if<HirVar>(&expr.kind)) {
            if (auto* b = lookup(var->name)) {
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

    Type check_field(HirFieldAccess& field, std::size_t offset) {
        const Type base_ty = check_expr(*field.base);
        const StructInfo* st = struct_of(base_ty);
        if (!st) {
            if (base_ty != Type::error()) {
                error(offset, "field access requires a struct, found '" + type_name(base_ty) + "'");
            }
            return Type::error();
        }
        if (st->opaque) {
            error(offset, "cannot access fields of opaque type '" + type_name(base_ty) + "'");
            return Type::error();
        }

        const FieldInfo* info = find_field(*st, field.name);
        if (!info) {
            error(offset, "unknown field '" + field.name + "' on '" + type_name(base_ty) + "'");
            return Type::error();
        }
        return info->ty;
    }

    Type check_struct_lit(HirStructLit& lit, std::size_t offset) {
        auto it = structs_.find(lit.name);
        if (it == structs_.end()) {
            error(offset, "unknown struct '" + lit.name + "'");
            for (auto& field : lit.fields) {
                check_expr(*field.value);
            }
            return Type::error();
        }

        const StructInfo& st = it->second;
        if (st.opaque) {
            error(offset, "cannot construct opaque type '" + lit.name + "'");
            for (auto& field : lit.fields) {
                check_expr(*field.value);
            }
            return Type::error();
        }
        std::vector<bool> seen(st.fields.size(), false);

        for (auto& field : lit.fields) {
            const Type value_ty = check_expr(*field.value);
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
            expect_expr(*field.value, st.fields[index].second.ty, field.value->offset, "field");
        }

        for (std::size_t i = 0; i < st.fields.size(); ++i) {
            if (!seen[i]) {
                error(offset, "missing field '" + st.fields[i].first + "' in '" + lit.name + "'");
            }
        }

        return Type::named(lit.name);
    }

    Type check_method_call(HirMethodCall& call, std::size_t offset) {
        Type recv_ty;
        bool associated = false;
        if (auto* var = std::get_if<HirVar>(&call.receiver->kind)) {
            if (!lookup(var->name) && (structs_.contains(var->name) || variants_.contains(var->name) ||
                                       c_enums_.contains(var->name))) {
                associated = true;
                recv_ty = Type::named(var->name);
                call.receiver->ty = recv_ty;
            }
        }
        if (!associated) {
            recv_ty = check_expr(*call.receiver);
        }
        call.associated = associated;

        if (recv_ty.kind != TypeKind::Named) {
            if (recv_ty != Type::error()) {
                error(offset, "method call requires a struct, found '" + type_name(recv_ty) + "'");
            }
            for (auto& arg : call.args) {
                check_expr(*arg);
            }
            return Type::error();
        }

        auto type_it = methods_.find(recv_ty.name);
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

        const MethodSig& sig = method_it->second;
        if (associated && sig.self_kind != SelfKind::None) {
            error(offset, "cannot call instance method '" + call.method + "' on type '" + recv_ty.name + "'");
        }
        if (!associated && sig.self_kind == SelfKind::Mut && !is_mut_place(*call.receiver)) {
            error(offset, "cannot call '" + call.method + "' on an immutable receiver");
        }

        for (auto& arg : call.args) {
            check_expr(*arg);
        }

        std::unordered_map<std::string, Type> mapping;
        bind_or_infer_type_args(sig.type_params, call.type_args, sig.params, call.args, offset, mapping);

        if (call.args.size() != sig.params.size()) {
            error(offset, "method '" + call.method + "' expects " + std::to_string(sig.params.size()) +
                              " argument(s), got " + std::to_string(call.args.size()));
        }

        const std::size_t n = std::min(call.args.size(), sig.params.size());
        for (std::size_t i = 0; i < n; ++i) {
            expect_expr(*call.args[i], subst_type(sig.params[i], mapping), call.args[i]->offset, "argument");
        }
        return subst_type(sig.ret, mapping);
    }

    Type check_field_assign(HirFieldAssign& as, std::size_t offset) {
        const Type base_ty = check_expr(*as.base);
        const Type value_ty = check_expr(*as.value);
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

        expect_expr(*as.value, info->ty, as.value->offset, "assignment");
        return info->ty;
    }

    Type check_index(HirIndex& idx, std::size_t offset) {
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

    Type check_index_assign(HirIndexAssign& as, std::size_t offset) {
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

    Type check_list_lit(HirListLit& lit, std::size_t offset) {
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

    Type check_dict_lit(HirDictLit& lit, std::size_t offset) {
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

    Type check_enum_lit(HirEnumLit& lit, std::size_t offset) {
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

    Type check_match(HirMatch& match, std::size_t offset) {
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
            auto v_it = variants_.find(scrut.name);
            if (v_it != variants_.end()) {
                adt = &v_it->second;
                covered.assign(adt->variants.size(), false);
            } else {
                auto c_it = c_enums_.find(scrut.name);
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

    void check_pat(HirPat& pat, const Type& scrut, const EnumInfo* en, const CEnumInfo* cen,
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
                        auto it = variants_.find(kind.enum_name);
                        if (it == variants_.end()) {
                            error(pat.offset, "unknown variant type '" + kind.enum_name + "'");
                            return;
                        }
                        used = &it->second;
                        if (scrut.kind == TypeKind::Named && scrut.name != kind.enum_name) {
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

    bool coerce_collection(HirExpr& expr, const Type& expected) {
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

    void expect_expr(HirExpr& expr, Type expected, std::size_t offset, const char* what) {
        const Type got = (expr.ty.kind == TypeKind::Unknown || expr.ty.kind == TypeKind::Error)
                             ? check_expr(expr)
                             : expr.ty;
        if (coerce_collection(expr, expected)) {
            return;
        }
        if (got == expected || got == Type::error() || expected == Type::error() ||
            got.kind == TypeKind::Never) {
            return;
        }
        if (coerce_lit(expr, expected)) {
            return;
        }
        error(offset, std::string(what) + " has type '" + type_name(got) + "', expected '" +
                          type_name(expected) + "'");
    }

    void expect_type(Type got, Type expected, std::size_t offset, const char* what) {
        if (got == Type::error() || expected == Type::error() || got == expected) {
            return;
        }
        error(offset, std::string(what) + " has type '" + type_name(got) + "', expected '" +
                          type_name(expected) + "'");
    }
};

}  // namespace

void typeck(const Source& src, HirModule& mod, DiagnosticEngine& diags) {
    TypeChecker{src, mod, diags}.run();
}

}  // namespace qpc

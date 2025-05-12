#include "compiler/typeck.hpp"

#include <algorithm>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace qpc {
namespace {

struct Binding {
    Type ty;
    bool mut = false;
};

struct FnSig {
    std::vector<Type> params;
    Type ret = Type::unit();
    std::size_t offset = 0;
};

struct MethodSig {
    SelfKind self_kind = SelfKind::None;
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
};

struct VariantInfo {
    std::vector<std::pair<std::string, FieldInfo>> fields;
    bool tuple = false;
    std::size_t index = 0;
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
    }
    return "Add";
}

static bool is_op_trait(std::string_view name) {
    return name == "Add" || name == "Sub" || name == "Mul" || name == "Div" || name == "Rem" ||
           name == "Neg";
}

class TypeChecker {
public:
    TypeChecker(const Source& src, HirModule& mod, DiagnosticEngine& diags)
        : src_(src), mod_(mod), diags_(diags) {}

    void run() {
        collect_enum_names();
        collect_structs();
        collect_enums();
        collect_sigs();
        collect_methods();
        if (diags_.has_errors()) {
            return;
        }

        for (auto& fn : mod_.functions) {
            check_fn(fn);
        }
        for (auto& impl : mod_.impls) {
            for (auto& method : impl.methods) {
                check_fn(method);
            }
        }
    }

private:
    const Source& src_;
    HirModule& mod_;
    DiagnosticEngine& diags_;
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, EnumInfo> enums_;
    std::unordered_map<std::string, FnSig> sigs_;
    std::unordered_map<std::string, std::unordered_map<std::string, MethodSig>> methods_;
    std::unordered_map<std::string, std::unordered_map<std::string, MethodSig>> op_impls_;
    std::vector<std::unordered_map<std::string, Binding>> scopes_;
    Type current_ret_ = Type::unit();

    void error(std::size_t offset, std::string message) {
        diags_.error(src_, offset, std::move(message));
    }

    bool resolve_type(Type& ty, std::size_t offset) {
        if (ty.kind != TypeKind::Named) {
            return ty != Type::error();
        }
        if (!structs_.contains(ty.name) && !enums_.contains(ty.name)) {
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

    void collect_structs() {
        for (auto& st : mod_.structs) {
            if (structs_.contains(st.name)) {
                error(st.offset, "duplicate struct '" + st.name + "'");
                continue;
            }
            structs_.emplace(st.name, StructInfo{{}, st.offset});
        }

        for (auto& st : mod_.structs) {
            auto it = structs_.find(st.name);
            if (it == structs_.end() || it->second.offset != st.offset) {
                continue;
            }

            StructInfo& info = it->second;
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

    void collect_enum_names() {
        for (auto& en : mod_.enums) {
            if (enums_.contains(en.name)) {
                error(en.offset, "duplicate type '" + en.name + "'");
                continue;
            }
            enums_.emplace(en.name, EnumInfo{{}, en.offset});
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

    void collect_enums() {
        for (auto& en : mod_.enums) {
            auto it = enums_.find(en.name);
            if (it == enums_.end() || it->second.offset != en.offset) {
                continue;
            }
            if (structs_.contains(en.name)) {
                error(en.offset, "duplicate type '" + en.name + "'");
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
            sigs_.emplace(fn.name, std::move(sig));
        }
    }

    void collect_methods() {
        for (auto& impl : mod_.impls) {
            const bool known = structs_.contains(impl.type_name) || enums_.contains(impl.type_name);
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
                }
            },
            stmt.kind);
    }

    void check_let(std::size_t offset, HirLet& let) {
        const Type init_ty = check_expr(*let.init);
        if (let.ty.kind == TypeKind::Unknown) {
            let.ty = init_ty;
        } else {
            resolve_type(let.ty, offset);
            expect_expr(*let.init, let.ty, let.init->offset, "let initializer");
        }
        declare(let.name, Binding{let.ty, let.mut}, offset);
    }

    void check_return(std::size_t offset, HirReturn& ret) {
        if (ret.value) {
            expect_expr(*ret.value, current_ret_, ret.value->offset, "return value");
            return;
        }
        if (current_ret_ != Type::unit()) {
            error(offset, "missing return value");
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
                } else if constexpr (std::is_same_v<K, HirMatch>) {
                    ty = check_match(kind, expr.offset);
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

    Type check_unary(HirUnary& un, std::size_t offset) {
        const Type inner = check_expr(*un.operand);
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

        if (bin.op == BinOp::Add && lhs == Type::string() && rhs == Type::string()) {
            return Type::string();
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

    Type check_call(HirCall& call, std::size_t offset) {
        auto it = sigs_.find(call.callee);
        if (it == sigs_.end()) {
            error(offset, "unknown function '" + call.callee + "'");
            for (auto& arg : call.args) {
                check_expr(*arg);
            }
            return Type::error();
        }

        const FnSig& sig = it->second;
        if (call.args.size() != sig.params.size()) {
            error(offset, "function '" + call.callee + "' expects " + std::to_string(sig.params.size()) +
                              " argument(s), got " + std::to_string(call.args.size()));
        }

        const std::size_t n = std::min(call.args.size(), sig.params.size());
        for (std::size_t i = 0; i < call.args.size(); ++i) {
            const Type arg_ty = check_expr(*call.args[i]);
            if (i < n) {
            expect_expr(*call.args[i], sig.params[i], call.args[i]->offset, "argument");
            }
        }
        return sig.ret;
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
        const Type recv_ty = check_expr(*call.receiver);
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
        if (sig.self_kind == SelfKind::Mut && !is_mut_place(*call.receiver)) {
            error(offset, "cannot call '" + call.method + "' on an immutable receiver");
        }

        if (call.args.size() != sig.params.size()) {
            error(offset, "method '" + call.method + "' expects " + std::to_string(sig.params.size()) +
                              " argument(s), got " + std::to_string(call.args.size()));
        }

        const std::size_t n = std::min(call.args.size(), sig.params.size());
        for (std::size_t i = 0; i < call.args.size(); ++i) {
            const Type arg_ty = check_expr(*call.args[i]);
            if (i < n) {
            expect_expr(*call.args[i], sig.params[i], call.args[i]->offset, "argument");
            }
        }
        return sig.ret;
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

    Type check_enum_lit(HirEnumLit& lit, std::size_t offset) {
        auto it = enums_.find(lit.enum_name);
        if (it == enums_.end()) {
            error(offset, "unknown enum '" + lit.enum_name + "'");
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
        const EnumInfo* en = nullptr;
        if (scrut.kind == TypeKind::Named) {
            auto it = enums_.find(scrut.name);
            if (it != enums_.end()) {
                en = &it->second;
                covered.assign(en->variants.size(), false);
            }
        }

        for (auto& arm : match.arms) {
            push_scope();
            check_pat(*arm.pat, scrut, en, covered);
            check_expr(*arm.body);
            if (first) {
                result = arm.body->ty;
                first = false;
            } else {
                expect_expr(*arm.body, result, arm.body->offset, "match arm");
            }
            pop_scope();
        }

        if (en) {
            for (std::size_t i = 0; i < covered.size(); ++i) {
                if (!covered[i]) {
                    error(offset, "non-exhaustive match, missing '" + en->variants[i].first + "'");
                }
            }
        }
        return result;
    }

    void check_pat(HirPat& pat, const Type& scrut, const EnumInfo* en, std::vector<bool>& covered) {
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
                    declare(kind.name, Binding{scrut, false}, pat.offset);
                } else if constexpr (std::is_same_v<K, HirPatVariant>) {
                    const EnumInfo* used = en;
                    if (!kind.enum_name.empty()) {
                        auto it = enums_.find(kind.enum_name);
                        if (it == enums_.end()) {
                            error(pat.offset, "unknown enum '" + kind.enum_name + "'");
                            return;
                        }
                        used = &it->second;
                        if (scrut.kind == TypeKind::Named && scrut.name != kind.enum_name) {
                            error(pat.offset, "pattern has type '" + kind.enum_name + "', expected '" +
                                                  type_name(scrut) + "'");
                        }
                    }
                    if (!used) {
                        error(pat.offset, "variant pattern requires an enum");
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

    void expect_expr(HirExpr& expr, Type expected, std::size_t offset, const char* what) {
        const Type got = (expr.ty.kind == TypeKind::Unknown || expr.ty.kind == TypeKind::Error)
                             ? check_expr(expr)
                             : expr.ty;
        if (got == expected || got == Type::error() || expected == Type::error()) {
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

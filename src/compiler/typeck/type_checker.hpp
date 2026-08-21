#pragma once

#include "compiler/diagnostic.hpp"
#include "compiler/hir.hpp"
#include "compiler/source.hpp"
#include "compiler/type.hpp"
#include "compiler/typeck/typeck_support.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qpc::detail {

class TypeChecker {
public:
    TypeChecker(const Source& src, HirModule& mod, DiagnosticEngine& diags)
        : current_src_(&src), mod_(mod), diags_(diags) {}


    void run();

private:
    const Source* current_src_;
    HirModule& mod_;
    DiagnosticEngine& diags_;
    std::string current_prefix_;
    std::unordered_map<std::string, std::string> type_aliases_;
    std::unordered_map<std::string, std::string> static_aliases_;
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, CEnumInfo> c_enums_;
    std::unordered_map<std::string, EnumInfo> variants_;
    std::unordered_map<std::string, std::vector<FnSig>> sigs_;
    std::unordered_map<std::string, Binding> statics_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<MethodSig>>> methods_;
    std::unordered_map<std::string, std::unordered_map<std::string, MethodSig>> op_impls_;
    std::unordered_map<std::string, std::unordered_map<std::string, MethodSig>> trait_methods_;
    std::unordered_map<std::string, std::unordered_set<std::string>> trait_impls_;
    std::unordered_set<std::string> traits_;
    std::unordered_set<std::string> generic_params_;
    std::vector<std::unordered_map<std::string, Binding>> scopes_;
    std::vector<std::pair<bool, std::size_t>> closure_frames_;
    Type current_ret_ = Type::unit();
    int loop_depth_ = 0;


    void error(std::size_t offset, std::string message);

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

    struct PrefixGuard {
        std::string& slot;
        std::string prev;
        PrefixGuard(std::string& s, std::string next) : slot(s), prev(std::move(s)) {
            slot = std::move(next);
        }
        ~PrefixGuard() { slot = std::move(prev); }
    };


    static std::string qualify(const std::string& prefix, const std::string& name);


    static bool name_has_path(const std::string& name);


    bool is_registered_named(const std::string& name) const;


    std::string lookup_named(const std::string& name) const;


    bool known_named_type(const std::string& name) const;


    bool resolve_type(Type& ty, std::size_t offset);


    const FieldInfo* find_field(const StructInfo& st, const std::string& name) const;


    std::unordered_map<std::string, Type> struct_subst(const StructInfo& st, const Type& ty) const;


    bool iterator_item(const Type& iter_ty, std::size_t offset, Type& item);


    const StructInfo* struct_of(const Type& ty) const;


    static std::string path_leaf(const std::string& name);


    void collect_names(HirModule& m, const std::string& prefix);


    void collect_type_details(HirModule& m, const std::string& prefix);


    void apply_uses(HirModule& m, const std::string& prefix);


    void collect_sigs_tree(HirModule& m, const std::string& prefix);


    Type infer_static_init_type(const HirExpr& expr);


    void collect_statics(HirModule& m, const std::string& prefix);


    void check_tree(HirModule& m, const std::string& prefix);


    void collect_sigs_of(HirModule& m, const std::string& prefix);


    void collect_trait_methods_of(HirModule& m, const std::string& prefix);


    void collect_methods_of(HirModule& m, const std::string& prefix);


    void check_trait_impl(const HirImpl& impl, const std::string& trait);


    const VariantInfo* find_variant(const EnumInfo& en, const std::string& name) const;


    const CEnumMemberInfo* find_member(const CEnumInfo& en, const std::string& name) const;


    void check_c_abi_type(const Type& ty, std::size_t offset, const std::string& fn_name);


    static const char* binop_method_from_trait(const std::string& trait);


    void push_scope();


    void pop_scope();


    bool declare(const std::string& name, Binding binding, std::size_t offset);


    Binding* lookup(const std::string& name);


    void check_fn(HirFn& fn);


    static bool ends_with_return(const HirBlock& body);


    void check_stmt(HirStmt& stmt);


    void check_let(std::size_t offset, HirLet& let);


    void check_return(std::size_t offset, HirReturn& ret);


    Type check_expr(HirExpr& expr);


    Binding* lookup_static(const std::string& name);


    Binding* lookup_binding(const std::string& name);


    Type check_path_var(HirVar& var, HirExpr& expr);


    Type check_var(HirVar& var, HirExpr& expr);


    Type subst_type(Type t, const std::unordered_map<std::string, Type>& mapping) const;


    static bool is_generic_param(const std::vector<HirTypeParam>& tps, const std::string& name);


    bool unify_type(const Type& pattern, const Type& actual, const std::vector<HirTypeParam>& tps,
                    std::unordered_map<std::string, Type>& mapping);


    void check_type_arg_bounds(const std::vector<HirTypeParam>& tps, const std::vector<Type>& args,
                               std::size_t offset);


    void bind_type_args(const std::vector<HirTypeParam>& tps, std::vector<Type>& args, std::size_t offset,
                        std::unordered_map<std::string, Type>& mapping);


    void infer_type_args(const std::vector<HirTypeParam>& tps, std::vector<Type>& type_args,
                         const std::vector<Type>& params, std::vector<HirExprPtr>& args, std::size_t offset,
                         std::unordered_map<std::string, Type>& mapping);


    void bind_or_infer_type_args(const std::vector<HirTypeParam>& tps, std::vector<Type>& type_args,
                                 const std::vector<Type>& params, std::vector<HirExprPtr>& args,
                                 std::size_t offset, std::unordered_map<std::string, Type>& mapping);

    /// -1 = incompatible; higher is a better overload match (exact > coerce; concrete > generic).
    int overload_arg_score(const HirExpr& expr, const Type& expected) const;

    bool try_score_overload(const std::vector<HirTypeParam>& type_params, const std::vector<Type>& params,
                            std::vector<Type> type_args, const std::vector<HirExprPtr>& args, int& score,
                            const std::unordered_map<std::string, Type>* base_mapping,
                            std::unordered_map<std::string, Type>& mapping);

    const FnSig* resolve_fn_overload(const std::vector<FnSig>& candidates, const std::string& name,
                                     HirCall& call, std::size_t offset,
                                     std::unordered_map<std::string, Type>& mapping);

    const MethodSig* resolve_method_overload(const std::vector<MethodSig>& candidates,
                                             const std::string& method, HirMethodCall& call,
                                             std::size_t offset, std::unordered_map<std::string, Type>& mapping);

    static bool same_param_types(const std::vector<Type>& a, const std::vector<Type>& b);

    bool add_fn_sig(const std::string& key, FnSig sig, std::size_t offset, const std::string& display_name);

    bool add_method_sig(std::unordered_map<std::string, std::vector<MethodSig>>& table, const std::string& name,
                        MethodSig sig, std::size_t offset);


    Type check_if(HirIf& iff, std::size_t offset);


    Type check_range(HirRange& range, std::size_t offset);


    Type check_closure(HirClosure& clo, std::size_t offset);


    bool can_cast(const Type& from, const Type& to) const;


    Type check_cast(HirCast& c, std::size_t offset);


    Type check_unwrap(HirUnwrap& un, std::size_t offset);


    Type check_new(HirNew& n, std::size_t offset);


    static Type as_nullable(Type t);


    Type check_coalesce(HirCoalesce& c, std::size_t offset);


    Type check_try(HirTry& t, std::size_t offset);


    Type check_unary(HirUnary& un, std::size_t offset);


    bool coerce_lit(HirExpr& expr, Type expected);


    bool coerce_null_to_nullable(HirExpr& expr, Type want);


    Type check_binop(HirBinary& bin, std::size_t offset);


    Type check_fn_value_call(const Type& fn_ty, HirCall& call, std::size_t offset, std::string callee);


    Type check_math_builtin(HirCall& call, std::size_t offset);


    bool can_to_string(const Type& ty);


    Type check_to_string_builtin(HirCall& call, std::size_t offset);


    Type check_call(HirCall& call, HirExpr& expr);


    Type check_assign(HirAssign& as, std::size_t offset);


    bool is_mut_place(const HirExpr& expr);


    Type check_field(HirFieldAccess& field, std::size_t offset);


    Type check_struct_lit(HirStructLit& lit, std::size_t offset);


    Type check_method_call(HirMethodCall& call, std::size_t offset);


    Type check_field_assign(HirFieldAssign& as, std::size_t offset);


    Type check_index(HirIndex& idx, std::size_t offset);


    Type check_index_assign(HirIndexAssign& as, std::size_t offset);


    Type check_list_lit(HirListLit& lit, std::size_t offset);


    Type check_tuple_lit(HirTupleLit& lit, std::size_t offset);


    Type check_dict_lit(HirDictLit& lit, std::size_t offset);


    Type check_enum_lit(HirEnumLit& lit, std::size_t offset);


    Type check_match(HirMatch& match, std::size_t offset);


    void check_pat(HirPat& pat, const Type& scrut, const EnumInfo* en, const CEnumInfo* cen,
                   std::vector<bool>& covered);


    bool coerce_collection(HirExpr& expr, const Type& expected);


    void expect_expr(HirExpr& expr, Type expected, std::size_t offset, const char* what);


    void expect_type(Type got, Type expected, std::size_t offset, const char* what);
};

}  // namespace qpc::detail

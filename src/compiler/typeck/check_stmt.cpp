#include "compiler/typeck/type_checker.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace qpc::detail {

void TypeChecker::push_scope() { scopes_.emplace_back(); }

void TypeChecker::pop_scope() { scopes_.pop_back(); }

bool TypeChecker::declare(const std::string& name, Binding binding, std::size_t offset) {
        auto& top = scopes_.back();
        if (top.contains(name)) {
            error(offset, "duplicate variable '" + name + "'");
            return false;
        }
        top.emplace(name, binding);
        return true;
    }

void TypeChecker::check_fn(HirFn& fn) {
        current_ret_ = fn.return_ty;
        push_scope();
        generic_params_.clear();
        for (const auto& tp : fn.type_params) {
            generic_params_.insert(tp.name);
            if (tp.bound && !traits_.contains(*tp.bound) && !is_op_trait(*tp.bound)) {
                error(fn.offset, "unknown trait bound '" + *tp.bound + "'");
            }
        }

        Type self_ty = Type::named(lookup_named(fn.self_ty));
        if (fn.self_kind != SelfKind::None) {
            if (auto it = structs_.find(lookup_named(fn.self_ty)); it != structs_.end()) {
                for (const auto& tp : it->second.type_params) {
                    generic_params_.insert(tp.name);
                    self_ty.args.push_back(Type::named(tp.name));
                }
            }
            declare("self", Binding{self_ty, fn.self_kind == SelfKind::Mut}, fn.offset);
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

void TypeChecker::check_stmt(HirStmt& stmt) {
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
                    Type value = Type::error();
                    const bool pair = !kind.second.empty();
                    if (std::holds_alternative<HirRange>(kind.iter->kind)) {
                        if (pair) {
                            error(stmt.offset, "for-loop over a range binds one variable");
                        }
                        if (kind.mut_name || kind.mut_second) {
                            error(stmt.offset, "cannot mutate a range loop variable in place");
                        }
                        elem = iter_ty;
                    } else if (iter_ty.kind == TypeKind::List || iter_ty.kind == TypeKind::Array) {
                        if (pair) {
                            const Type& item = iter_ty.elem();
                            if (item.kind != TypeKind::Tuple || item.args.size() != 2) {
                                error(stmt.offset,
                                      "for-loop unpack requires a 2-tuple element, found '" +
                                          type_name(item) + "'");
                            } else {
                                elem = item.args[0];
                                value = item.args[1];
                            }
                        } else {
                            elem = iter_ty.elem();
                        }
                    } else if (iter_ty.kind == TypeKind::Dict) {
                        if (!pair) {
                            error(stmt.offset, "for-loop over a dict requires '(key, value)'");
                        }
                        if (kind.mut_name) {
                            error(stmt.offset, "cannot mutate dict keys");
                        }
                        elem = iter_ty.key();
                        value = iter_ty.value();
                    } else {
                        Type item = Type::error();
                        if (iterator_item(iter_ty, stmt.offset, item)) {
                            kind.by_next = true;
                            if (pair) {
                                if (item.kind != TypeKind::Tuple || item.args.size() != 2) {
                                    error(stmt.offset,
                                          "for-loop unpack requires a 2-tuple element, found '" +
                                              type_name(item) + "'");
                                } else {
                                    elem = item.args[0];
                                    value = item.args[1];
                                }
                            } else {
                                elem = item;
                            }
                        } else if (iter_ty != Type::error()) {
                            error(stmt.offset, "for-loop requires a list, array, dict, range or iterator");
                        }
                    }
                    ++loop_depth_;
                    push_scope();
                    if ((kind.mut_name || kind.mut_second) && !is_mut_place(*kind.iter) &&
                        !std::holds_alternative<HirListLit>(kind.iter->kind) &&
                        !std::holds_alternative<HirTupleLit>(kind.iter->kind) &&
                        !std::holds_alternative<HirDictLit>(kind.iter->kind) &&
                        !std::holds_alternative<HirStructLit>(kind.iter->kind) &&
                        !std::holds_alternative<HirCall>(kind.iter->kind) &&
                        !std::holds_alternative<HirMethodCall>(kind.iter->kind)) {
                        error(stmt.offset, "in-place for-loop needs a mutable place");
                    }
                    declare(kind.name, Binding{elem, kind.mut_name}, stmt.offset);
                    if (pair) {
                        declare(kind.second, Binding{value, kind.mut_second}, stmt.offset);
                    }
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

void TypeChecker::check_let(std::size_t offset, HirLet& let) {
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

void TypeChecker::check_return(std::size_t offset, HirReturn& ret) {
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

bool TypeChecker::iterator_item(const Type& iter_ty, std::size_t offset, Type& item) {
        if (iter_ty.kind != TypeKind::Named) {
            return false;
        }
        auto type_it = methods_.find(lookup_named(iter_ty.name));
        if (type_it == methods_.end()) {
            type_it = methods_.find(iter_ty.name);
        }
        if (type_it == methods_.end()) {
            return false;
        }
        auto method_it = type_it->second.find("next");
        if (method_it == type_it->second.end()) {
            return false;
        }
        const MethodSig* chosen = nullptr;
        for (const auto& sig : method_it->second) {
            if (sig.self_kind == SelfKind::Mut && sig.params.empty()) {
                if (chosen) {
                    error(offset, "ambiguous iterator next()");
                    return false;
                }
                chosen = &sig;
            }
        }
        if (!chosen) {
            error(offset, "iterator next() must take mut self and no arguments");
            return false;
        }
        std::unordered_map<std::string, Type> mapping;
        if (const StructInfo* st = struct_of(iter_ty)) {
            mapping = struct_subst(*st, iter_ty);
        }
        Type ret = subst_type(chosen->ret, mapping);
        if (ret.kind != TypeKind::Nullable) {
            error(offset, "iterator next() must return T?, found '" + type_name(ret) + "'");
            return false;
        }
        item = ret.elem();
        return true;
    }

}  // namespace qpc::detail

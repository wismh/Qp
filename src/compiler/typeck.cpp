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

class TypeChecker {
public:
    TypeChecker(const Source& src, HirModule& mod, DiagnosticEngine& diags)
        : src_(src), mod_(mod), diags_(diags) {}

    void run() {
        collect_sigs();
        if (diags_.has_errors()) {
            return;
        }

        for (auto& fn : mod_.functions) {
            check_fn(fn);
        }
    }

private:
    const Source& src_;
    HirModule& mod_;
    DiagnosticEngine& diags_;
    std::unordered_map<std::string, FnSig> sigs_;
    std::vector<std::unordered_map<std::string, Binding>> scopes_;
    Type current_ret_ = Type::unit();

    void error(std::size_t offset, std::string message) {
        diags_.error(src_, offset, std::move(message));
    }

    void collect_sigs() {
        for (const auto& fn : mod_.functions) {
            if (sigs_.contains(fn.name)) {
                error(fn.offset, "duplicate function '" + fn.name + "'");
                continue;
            }

            FnSig sig;
            sig.ret = fn.return_ty;
            sig.offset = fn.offset;
            sig.params.reserve(fn.params.size());
            for (const auto& p : fn.params) {
                sig.params.push_back(p.ty);
            }
            sigs_.emplace(fn.name, std::move(sig));
        }
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

        for (const auto& p : fn.params) {
            declare(p.name, Binding{p.ty, true}, p.offset);
        }

        for (auto& stmt : fn.body.stmts) {
            check_stmt(*stmt);
        }

        if (fn.body.tail) {
            const Type tail_ty = check_expr(*fn.body.tail);
            expect_type(tail_ty, fn.return_ty, fn.body.tail->offset, "function body");
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
        if (let.ty == Type::unknown()) {
            let.ty = init_ty;
        } else {
            expect_type(init_ty, let.ty, let.init->offset, "let initializer");
        }
        declare(let.name, Binding{let.ty, let.mut}, offset);
    }

    void check_return(std::size_t offset, HirReturn& ret) {
        if (ret.value) {
            expect_type(check_expr(*ret.value), current_ret_, ret.value->offset, "return value");
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
                    ty = Type::i32();
                } else if constexpr (std::is_same_v<K, HirLitFloat>) {
                    ty = Type::f32();
                } else if constexpr (std::is_same_v<K, HirVar>) {
                    ty = check_var(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirBinary>) {
                    ty = check_binop(kind.op, check_expr(*kind.lhs), check_expr(*kind.rhs), expr.offset);
                } else if constexpr (std::is_same_v<K, HirUnary>) {
                    ty = check_unary(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirCall>) {
                    ty = check_call(kind, expr.offset);
                } else if constexpr (std::is_same_v<K, HirAssign>) {
                    ty = check_assign(kind, expr.offset);
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
        if (inner == Type::i32() || inner == Type::f32()) {
            return inner;
        }
        if (inner != Type::error()) {
            error(offset, "unary '-' requires i32 or f32");
        }
        return Type::error();
    }

    Type check_binop(BinOp op, Type lhs, Type rhs, std::size_t offset) {
        if (lhs == Type::error() || rhs == Type::error()) {
            return Type::error();
        }
        if (lhs != rhs) {
            error(offset, "cannot apply operator to '" + std::string(type_name(lhs)) + "' and '" +
                              std::string(type_name(rhs)) + "'");
            return Type::error();
        }
        if (op == BinOp::Mod) {
            if (lhs != Type::i32()) {
                error(offset, "'%' requires i32 operands");
                return Type::error();
            }
            return Type::i32();
        }
        if (lhs != Type::i32() && lhs != Type::f32()) {
            error(offset, "arithmetic requires i32 or f32");
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
                expect_type(arg_ty, sig.params[i], call.args[i]->offset, "argument");
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

        expect_type(value_ty, b->ty, as.value->offset, "assignment");
        return b->ty;
    }

    void expect_type(Type got, Type expected, std::size_t offset, const char* what) {
        if (got == Type::error() || expected == Type::error() || got == expected) {
            return;
        }
        error(offset, std::string(what) + " has type '" + std::string(type_name(got)) + "', expected '" +
                          std::string(type_name(expected)) + "'");
    }
};

}  // namespace

void typeck(const Source& src, HirModule& mod, DiagnosticEngine& diags) {
    TypeChecker{src, mod, diags}.run();
}

}  // namespace qpc

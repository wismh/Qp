#pragma once

#include "compiler/ast.hpp"

#include <memory>
#include <utility>

namespace qpc::detail {

template <class Kind>
inline ExprPtr make_expr(std::size_t offset, Kind kind) {
    auto expr = std::make_unique<Expr>();
    expr->offset = offset;
    expr->kind = std::move(kind);
    return expr;
}

template <class Kind>
inline StmtPtr make_stmt(std::size_t offset, Kind kind) {
    auto stmt = std::make_unique<Stmt>();
    stmt->offset = offset;
    stmt->kind = std::move(kind);
    return stmt;
}

}  // namespace qpc::detail

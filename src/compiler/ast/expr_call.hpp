#pragma once

#include "compiler/ast/fwd.hpp"
#include "compiler/ast/type_expr.hpp"
#include <vector>

namespace qpc {

struct ExprCall {
    ExprPtr callee;
    std::vector<TypeExpr> type_args;
    std::vector<ExprPtr> args;
};

}  // namespace qpc

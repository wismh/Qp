#pragma once

#include "compiler/ast/fwd.hpp"
#include "compiler/ast/type_expr.hpp"

namespace qpc {

struct ExprCast {
    ExprPtr expr;
    TypeExpr ty;
};

}  // namespace qpc

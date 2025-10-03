#pragma once

#include "compiler/ast/closure_param.hpp"
#include "compiler/ast/fwd.hpp"
#include "compiler/ast/type_expr.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace qpc {

struct ExprClosure {
    bool by_ref = false;
    std::vector<ClosureParam> params;
    std::optional<TypeExpr> return_ty;
    std::unique_ptr<Block> body;
};

}  // namespace qpc

#pragma once

#include "compiler/ast/fwd.hpp"
#include "compiler/ast/type_expr.hpp"
#include <optional>
#include <string>

namespace qpc {

struct StmtLet {
    bool mut = false;
    std::string name;
    std::optional<TypeExpr> ty;
    ExprPtr init;
};

}  // namespace qpc

#pragma once

#include "compiler/ast/fwd.hpp"
#include "compiler/ast/struct_lit_field.hpp"
#include "compiler/ast/type_expr.hpp"
#include <string>
#include <vector>

namespace qpc {

struct ExprNew {
    std::string name;
    std::vector<std::string> path;
    std::vector<TypeExpr> type_args;
    std::vector<StructLitField> fields;
};

}  // namespace qpc

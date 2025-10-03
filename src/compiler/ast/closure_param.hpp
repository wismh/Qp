#pragma once

#include "compiler/ast/type_expr.hpp"
#include <cstddef>
#include <string>

namespace qpc {

struct ClosureParam {
    std::string name;
    TypeExpr ty;
    std::size_t offset = 0;
};

}  // namespace qpc

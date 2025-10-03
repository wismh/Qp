#pragma once

#include "compiler/ast/fwd.hpp"

namespace qpc {

struct ExprAssign {
    ExprPtr lhs;
    ExprPtr rhs;
};

}  // namespace qpc

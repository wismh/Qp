#pragma once

#include "compiler/ast/fwd.hpp"

namespace qpc {

struct ExprCoalesce {
    ExprPtr lhs;
    ExprPtr rhs;
};

}  // namespace qpc

#pragma once

#include "compiler/hir/fwd.hpp"

namespace qpc {

struct HirCoalesce {
    HirExprPtr lhs;
    HirExprPtr rhs;
};

}  // namespace qpc

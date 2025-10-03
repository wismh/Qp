#pragma once

#include "compiler/hir/fwd.hpp"

namespace qpc {

struct HirIndexAssign {
    HirExprPtr base;
    HirExprPtr index;
    HirExprPtr value;
};

}  // namespace qpc

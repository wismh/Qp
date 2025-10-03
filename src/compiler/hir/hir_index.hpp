#pragma once

#include "compiler/hir/fwd.hpp"

namespace qpc {

struct HirIndex {
    HirExprPtr base;
    HirExprPtr index;
};

}  // namespace qpc

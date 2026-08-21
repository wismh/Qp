#pragma once

#include "compiler/hir/fwd.hpp"
#include <vector>

namespace qpc {

struct HirTupleLit {
    std::vector<HirExprPtr> elems;
};

}  // namespace qpc

#pragma once

#include "compiler/hir/fwd.hpp"
#include <vector>

namespace qpc {

struct HirListLit {
    std::vector<HirExprPtr> elems;
    bool array = false;
};

}  // namespace qpc

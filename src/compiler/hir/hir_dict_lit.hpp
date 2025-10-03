#pragma once

#include "compiler/hir/fwd.hpp"
#include <utility>
#include <vector>

namespace qpc {

struct HirDictLit {
    std::vector<std::pair<HirExprPtr, HirExprPtr>> entries;
};

}  // namespace qpc

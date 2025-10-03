#pragma once

#include "compiler/hir/fwd.hpp"
#include <cstddef>
#include <vector>

namespace qpc {

struct HirBlock {
    std::vector<HirStmtPtr> stmts;
    HirExprPtr tail;
    std::size_t offset = 0;
};

}  // namespace qpc

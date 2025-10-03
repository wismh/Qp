#pragma once

#include "compiler/hir/fwd.hpp"
#include <vector>

namespace qpc {

struct HirWhile {
    HirExprPtr cond;
    std::vector<HirStmtPtr> stmts;
    HirExprPtr tail;
};

}  // namespace qpc

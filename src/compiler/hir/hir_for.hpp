#pragma once

#include "compiler/hir/fwd.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirFor {
    std::string name;
    std::string second;
    HirExprPtr iter;
    std::vector<HirStmtPtr> stmts;
    HirExprPtr tail;
};

}  // namespace qpc

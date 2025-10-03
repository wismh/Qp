#pragma once

#include "compiler/hir/fwd.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirIf {
    HirExprPtr cond;
    std::string let_name;
    std::vector<HirStmtPtr> then_stmts;
    HirExprPtr then_tail;
    HirExprPtr else_expr;
};

}  // namespace qpc

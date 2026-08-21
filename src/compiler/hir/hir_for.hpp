#pragma once

#include "compiler/hir/fwd.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirFor {
    std::string name;
    std::string second;
    bool mut_name = false;
    bool mut_second = false;
    HirExprPtr iter;
    std::vector<HirStmtPtr> stmts;
    HirExprPtr tail;
    bool by_next = false;
};

}  // namespace qpc

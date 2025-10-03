#pragma once

#include "compiler/hir/fwd.hpp"
#include "compiler/type.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirCall {
    std::string callee;
    HirExprPtr callee_expr;
    std::vector<Type> type_args;
    std::vector<HirExprPtr> args;
};

}  // namespace qpc

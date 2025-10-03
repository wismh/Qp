#pragma once

#include "compiler/hir/fwd.hpp"
#include "compiler/type.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirMethodCall {
    HirExprPtr receiver;
    std::string method;
    std::vector<Type> type_args;
    std::vector<HirExprPtr> args;
    bool associated = false;
    bool null_safe = false;
    bool wrap_ret = false;
};

}  // namespace qpc

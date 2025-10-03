#pragma once

#include "compiler/hir/fwd.hpp"
#include <string>

namespace qpc {

struct HirFieldAssign {
    HirExprPtr base;
    std::string field;
    HirExprPtr value;
};

}  // namespace qpc

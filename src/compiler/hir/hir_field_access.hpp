#pragma once

#include "compiler/hir/fwd.hpp"
#include <string>

namespace qpc {

struct HirFieldAccess {
    HirExprPtr base;
    std::string name;
    bool null_safe = false;
    bool take_addr = false;
};

}  // namespace qpc

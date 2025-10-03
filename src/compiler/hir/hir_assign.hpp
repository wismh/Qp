#pragma once

#include "compiler/hir/fwd.hpp"
#include <string>

namespace qpc {

struct HirAssign {
    std::string name;
    HirExprPtr value;
};

}  // namespace qpc

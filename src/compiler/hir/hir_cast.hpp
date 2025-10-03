#pragma once

#include "compiler/hir/fwd.hpp"
#include "compiler/type.hpp"

namespace qpc {

struct HirCast {
    HirExprPtr expr;
    Type ty;
};

}  // namespace qpc

#pragma once

#include "compiler/hir/fwd.hpp"

namespace qpc {

struct HirMatchArm {
    HirPatPtr pat;
    HirExprPtr body;
};

}  // namespace qpc

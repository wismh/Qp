#pragma once

#include "compiler/hir/fwd.hpp"
#include "compiler/hir/hir_match_arm.hpp"
#include <vector>

namespace qpc {

struct HirMatch {
    HirExprPtr scrutinee;
    std::vector<HirMatchArm> arms;
};

}  // namespace qpc

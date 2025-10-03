#pragma once

#include "compiler/ast/fwd.hpp"
#include "compiler/ast/match_arm.hpp"
#include <vector>

namespace qpc {

struct ExprMatch {
    ExprPtr scrutinee;
    std::vector<MatchArm> arms;
};

}  // namespace qpc

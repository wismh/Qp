#pragma once

#include "compiler/ast/fwd.hpp"

namespace qpc {

struct MatchArm {
    PatPtr pat;
    ExprPtr body;
};

}  // namespace qpc

#pragma once

#include "compiler/hir/fwd.hpp"

namespace qpc {

struct HirRange {
    HirExprPtr start;
    HirExprPtr end;
};

}  // namespace qpc

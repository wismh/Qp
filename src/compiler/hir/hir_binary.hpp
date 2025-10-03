#pragma once

#include "compiler/hir/bin_op.hpp"
#include "compiler/hir/fwd.hpp"

namespace qpc {

struct HirBinary {
    BinOp op = BinOp::Add;
    HirExprPtr lhs;
    HirExprPtr rhs;
};

}  // namespace qpc

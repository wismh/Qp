#pragma once

#include "compiler/hir/fwd.hpp"
#include "compiler/hir/un_op.hpp"

namespace qpc {

struct HirUnary {
    UnOp op = UnOp::Neg;
    HirExprPtr operand;
};

}  // namespace qpc

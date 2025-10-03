#pragma once

#include "compiler/ast/fwd.hpp"

namespace qpc {

struct ExprIndex {
    ExprPtr base;
    ExprPtr index;
};

}  // namespace qpc

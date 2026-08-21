#pragma once

#include "compiler/ast/fwd.hpp"
#include <vector>

namespace qpc {

struct ExprTuple {
    std::vector<ExprPtr> elems;
};

}  // namespace qpc

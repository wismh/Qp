#pragma once

#include "compiler/ast/fwd.hpp"
#include <vector>

namespace qpc {

struct ExprListLit {
    std::vector<ExprPtr> elems;
};

}  // namespace qpc

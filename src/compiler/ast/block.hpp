#pragma once

#include "compiler/ast/fwd.hpp"
#include <cstddef>
#include <vector>

namespace qpc {

struct Block {
    std::vector<StmtPtr> stmts;
    ExprPtr tail;
    std::size_t offset = 0;
};

}  // namespace qpc

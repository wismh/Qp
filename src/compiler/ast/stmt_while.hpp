#pragma once

#include "compiler/ast/fwd.hpp"
#include <memory>

namespace qpc {

struct StmtWhile {
    ExprPtr cond;
    std::unique_ptr<Block> body;
};

}  // namespace qpc

#pragma once

#include "compiler/ast/fwd.hpp"
#include <memory>
#include <string>

namespace qpc {

struct ExprIf {
    ExprPtr cond;
    std::string let_name;
    std::unique_ptr<Block> then_block;
    ExprPtr else_expr;
};

}  // namespace qpc

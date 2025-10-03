#pragma once

#include "compiler/ast/fwd.hpp"
#include "compiler/token.hpp"

namespace qpc {

struct ExprUnary {
    TokenKind op = TokenKind::Minus;
    ExprPtr operand;
};

}  // namespace qpc

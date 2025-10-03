#pragma once

#include "compiler/ast/fwd.hpp"
#include "compiler/token.hpp"

namespace qpc {

struct ExprBinary {
    TokenKind op = TokenKind::Plus;
    ExprPtr lhs;
    ExprPtr rhs;
};

}  // namespace qpc

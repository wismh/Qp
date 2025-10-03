#pragma once

#include "compiler/ast/fwd.hpp"

namespace qpc {

struct ExprDictEntry {
    ExprPtr key;
    ExprPtr value;
};

}  // namespace qpc

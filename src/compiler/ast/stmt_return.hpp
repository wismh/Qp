#pragma once

#include "compiler/ast/fwd.hpp"

namespace qpc {

struct StmtReturn {
    ExprPtr value;
};

}  // namespace qpc

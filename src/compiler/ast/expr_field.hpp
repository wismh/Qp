#pragma once

#include "compiler/ast/fwd.hpp"
#include <string>

namespace qpc {

struct ExprField {
    ExprPtr base;
    std::string name;
    bool null_safe = false;
};

}  // namespace qpc

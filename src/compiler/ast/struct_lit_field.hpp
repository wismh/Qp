#pragma once

#include "compiler/ast/fwd.hpp"
#include <string>

namespace qpc {

struct StructLitField {
    std::string name;
    ExprPtr value;
};

}  // namespace qpc

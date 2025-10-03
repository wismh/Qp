#pragma once

#include "compiler/hir/fwd.hpp"
#include <string>

namespace qpc {

struct HirStructLitField {
    std::string name;
    HirExprPtr value;
};

}  // namespace qpc

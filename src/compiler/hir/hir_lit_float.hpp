#pragma once

#include "compiler/type.hpp"

namespace qpc {

struct HirLitFloat {
    double value = 0.0;
    bool unsuffixed = true;
    Type ty = Type::f32();
};

}  // namespace qpc

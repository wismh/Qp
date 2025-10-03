#pragma once

#include "compiler/type.hpp"
#include <cstdint>

namespace qpc {

struct HirLitInt {
    std::int64_t value = 0;
    bool unsuffixed = true;
    Type ty = Type::i32();
};

}  // namespace qpc

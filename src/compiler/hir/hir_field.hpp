#pragma once

#include "compiler/type.hpp"
#include <cstddef>
#include <string>

namespace qpc {

struct HirField {
    bool mut = false;
    std::string name;
    Type ty;
    std::size_t offset = 0;
};

}  // namespace qpc

#pragma once

#include "compiler/type.hpp"
#include <cstddef>
#include <string>

namespace qpc {

struct HirParam {
    std::string name;
    Type ty;
    bool mut = false;
    bool pack = false;
    std::size_t offset = 0;
};

}  // namespace qpc

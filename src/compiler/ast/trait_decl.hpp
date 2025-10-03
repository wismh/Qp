#pragma once

#include "compiler/ast/trait_method.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct TraitDecl {
    bool pub = false;
    std::string name;
    std::vector<TraitMethod> methods;
    std::size_t offset = 0;
};

}  // namespace qpc

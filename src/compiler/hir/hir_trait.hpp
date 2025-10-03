#pragma once

#include "compiler/hir/hir_trait_method.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct HirTrait {
    bool pub = false;
    std::string name;
    std::vector<HirTraitMethod> methods;
    std::size_t offset = 0;
};

}  // namespace qpc

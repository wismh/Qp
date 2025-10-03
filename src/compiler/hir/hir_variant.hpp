#pragma once

#include "compiler/hir/hir_enum_variant.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct HirVariant {
    bool pub = false;
    std::string name;
    std::vector<HirEnumVariant> variants;
    std::size_t offset = 0;
};

}  // namespace qpc

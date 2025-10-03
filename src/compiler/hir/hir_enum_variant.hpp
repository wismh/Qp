#pragma once

#include "compiler/hir/hir_field.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct HirEnumVariant {
    std::string name;
    std::vector<HirField> fields;
    bool tuple = false;
    std::size_t offset = 0;
};

}  // namespace qpc

#pragma once

#include "compiler/hir/hir_struct_lit_field.hpp"
#include "compiler/type.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirStructLit {
    std::string name;
    std::vector<Type> type_args;
    std::vector<HirStructLitField> fields;
};

}  // namespace qpc

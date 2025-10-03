#pragma once

#include "compiler/hir/fwd.hpp"
#include "compiler/hir/hir_struct_lit_field.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirEnumLit {
    std::string enum_name;
    std::string variant;
    bool tuple = false;
    std::vector<HirStructLitField> fields;
    std::vector<HirExprPtr> args;
};

}  // namespace qpc

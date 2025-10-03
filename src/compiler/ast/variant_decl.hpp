#pragma once

#include "compiler/ast/field_decl.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct VariantDecl {
    std::string name;
    std::vector<FieldDecl> fields;
    bool tuple = false;
    std::size_t offset = 0;
};

}  // namespace qpc

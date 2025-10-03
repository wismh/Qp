#pragma once

#include "compiler/ast/variant_decl.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct VariantTypeDecl {
    bool pub = false;
    std::string name;
    std::vector<VariantDecl> variants;
    std::size_t offset = 0;
};

}  // namespace qpc

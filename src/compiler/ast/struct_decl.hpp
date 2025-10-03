#pragma once

#include "compiler/ast/field_decl.hpp"
#include "compiler/ast/type_param.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct StructDecl {
    bool pub = false;
    bool is_extern = false;
    bool opaque = false;
    std::string name;
    std::vector<TypeParam> type_params;
    std::vector<FieldDecl> fields;
    std::size_t offset = 0;
};

}  // namespace qpc

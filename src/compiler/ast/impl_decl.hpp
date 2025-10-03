#pragma once

#include "compiler/ast/fn_decl.hpp"
#include "compiler/ast/type_param.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace qpc {

struct ImplDecl {
    std::optional<std::string> trait_name;
    std::string type_name;
    std::vector<TypeParam> type_params;
    std::vector<FnDecl> methods;
    std::size_t offset = 0;
};

}  // namespace qpc

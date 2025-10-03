#pragma once

#include "compiler/ast/param.hpp"
#include "compiler/ast/self_param.hpp"
#include "compiler/ast/type_expr.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace qpc {

struct TraitMethod {
    SelfParam self_param = SelfParam::None;
    std::string name;
    std::vector<Param> params;
    std::optional<TypeExpr> return_ty;
    std::size_t offset = 0;
};

}  // namespace qpc

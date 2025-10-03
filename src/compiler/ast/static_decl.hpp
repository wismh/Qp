#pragma once

#include "compiler/ast/fwd.hpp"
#include "compiler/ast/type_expr.hpp"
#include <cstddef>
#include <optional>
#include <string>

namespace qpc {

struct StaticDecl {
    bool pub = false;
    bool mut = false;
    bool is_extern = false;
    std::string name;
    std::optional<TypeExpr> ty;
    ExprPtr init;
    std::size_t offset = 0;
};

}  // namespace qpc

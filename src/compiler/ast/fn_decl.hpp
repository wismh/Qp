#pragma once

#include "compiler/ast/abi.hpp"
#include "compiler/ast/block.hpp"
#include "compiler/ast/param.hpp"
#include "compiler/ast/self_param.hpp"
#include "compiler/ast/type_expr.hpp"
#include "compiler/ast/type_param.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace qpc {


struct FnDecl {
    bool pub = false;
    bool is_extern = false;
    Abi abi = Abi::Qplus;
    SelfParam self_param = SelfParam::None;
    std::string name;
    std::vector<TypeParam> type_params;
    std::vector<Param> params;
    std::optional<TypeExpr> return_ty;
    Block body;
    std::size_t offset = 0;
};

}  // namespace qpc

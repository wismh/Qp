#pragma once

#include "compiler/hir/hir_block.hpp"
#include "compiler/hir/hir_param.hpp"
#include "compiler/type.hpp"
#include <vector>

namespace qpc {

struct HirClosure {
    bool by_ref = false;
    std::vector<HirParam> params;
    Type return_ty = Type::unknown();
    HirBlock body;
};

}  // namespace qpc

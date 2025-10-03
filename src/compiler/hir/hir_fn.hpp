#pragma once

#include "compiler/hir/hir_block.hpp"
#include "compiler/hir/hir_param.hpp"
#include "compiler/hir/hir_type_param.hpp"
#include "compiler/hir/self_kind.hpp"
#include "compiler/type.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {


struct HirFn {
    bool pub = false;
    bool is_extern = false;
    bool c_abi = false;
    SelfKind self_kind = SelfKind::None;
    std::string self_ty;
    std::string name;
    std::vector<HirTypeParam> type_params;
    std::vector<HirParam> params;
    Type return_ty = Type::unit();
    HirBlock body;
    std::size_t offset = 0;
};

}  // namespace qpc

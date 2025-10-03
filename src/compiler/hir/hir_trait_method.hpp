#pragma once

#include "compiler/hir/hir_param.hpp"
#include "compiler/hir/self_kind.hpp"
#include "compiler/type.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirTraitMethod {
    SelfKind self_kind = SelfKind::None;
    std::string name;
    std::vector<HirParam> params;
    Type return_ty = Type::unit();
};

}  // namespace qpc

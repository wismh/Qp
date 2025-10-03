#pragma once

#include "compiler/hir/fwd.hpp"
#include "compiler/type.hpp"
#include <string>

namespace qpc {

struct HirLet {
    bool mut = false;
    std::string name;
    Type ty;
    HirExprPtr init;
};

}  // namespace qpc

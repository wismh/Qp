#pragma once

#include "compiler/hir/fwd.hpp"
#include "compiler/type.hpp"
#include <cstddef>
#include <string>

namespace qpc {

struct HirStatic {
    bool pub = false;
    bool mut = false;
    bool is_extern = false;
    std::string name;
    Type ty;
    HirExprPtr init;
    std::size_t offset = 0;
};

}  // namespace qpc

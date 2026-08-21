#pragma once

#include "compiler/type.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirVar {
    std::string name;
    std::vector<Type> type_args;
    bool fn_value = false;
    bool pack_expand = false;
};

}  // namespace qpc

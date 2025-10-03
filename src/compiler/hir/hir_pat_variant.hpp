#pragma once

#include "compiler/hir/fwd.hpp"
#include <string>
#include <vector>

namespace qpc {

struct HirPatVariant {
    std::string enum_name;
    std::string variant;
    bool tuple = false;
    std::vector<std::string> fields;
    std::vector<HirPatPtr> args;
};

}  // namespace qpc

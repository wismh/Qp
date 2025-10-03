#pragma once

#include "compiler/ast/fwd.hpp"
#include <string>
#include <vector>

namespace qpc {

struct PatVariant {
    std::vector<std::string> path;
    std::vector<std::string> fields;
    std::vector<PatPtr> args;
    bool tuple = false;
};

}  // namespace qpc

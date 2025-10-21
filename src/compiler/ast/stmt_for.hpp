#pragma once

#include "compiler/ast/fwd.hpp"
#include <memory>
#include <string>

namespace qpc {

struct StmtFor {
    std::string name;
    std::string second;
    bool mut_name = false;
    bool mut_second = false;
    ExprPtr iter;
    std::unique_ptr<Block> body;
};

}  // namespace qpc

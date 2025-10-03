#pragma once

#include "compiler/ast/expr_dict_entry.hpp"
#include <vector>

namespace qpc {

struct ExprDictLit {
    std::vector<ExprDictEntry> entries;
};

}  // namespace qpc

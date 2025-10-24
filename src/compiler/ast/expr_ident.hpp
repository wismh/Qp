#pragma once

#include <string>

namespace qpc {

struct ExprIdent {
    std::string name;
    bool pack_expand = false;
};

}  // namespace qpc

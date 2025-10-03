#pragma once

#include "compiler/ast/enum_member.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct EnumDecl {
    bool pub = false;
    std::string name;
    std::vector<EnumMember> members;
    std::size_t offset = 0;
};

}  // namespace qpc

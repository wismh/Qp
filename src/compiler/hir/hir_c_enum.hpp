#pragma once

#include "compiler/hir/hir_c_enum_member.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct HirCEnum {
    bool pub = false;
    std::string name;
    std::vector<HirCEnumMember> members;
    std::size_t offset = 0;
};

}  // namespace qpc

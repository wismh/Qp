#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace qpc {

struct HirCEnumMember {
    std::string name;
    std::int64_t value = 0;
    std::size_t offset = 0;
};

}  // namespace qpc

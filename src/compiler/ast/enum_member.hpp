#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace qpc {

struct EnumMember {
    std::string name;
    std::optional<std::int64_t> value;
    std::size_t offset = 0;
};

}  // namespace qpc

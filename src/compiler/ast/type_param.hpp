#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace qpc {


struct TypeParam {
    std::string name;
    std::optional<std::string> bound;
    bool pack = false;
    std::size_t offset = 0;
};

}  // namespace qpc

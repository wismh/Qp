#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct HirUse {
    std::vector<std::string> path;
    bool glob = false;
    std::size_t offset = 0;
};

}  // namespace qpc

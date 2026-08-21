#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace qpc {

struct UseDecl {
    std::vector<std::string> path;
    std::vector<std::string> names;
    bool glob = false;
    bool from_load = false;
    std::size_t offset = 0;
};

}  // namespace qpc

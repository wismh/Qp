#pragma once

#include <optional>
#include <string>

namespace qpc {

struct HirTypeParam {
    std::string name;
    std::optional<std::string> bound;
};

}  // namespace qpc

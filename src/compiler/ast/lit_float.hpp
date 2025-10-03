#pragma once

#include <optional>
#include <string>

namespace qpc {

struct LitFloat {
    std::string raw;
    std::optional<std::string> suffix;
};

}  // namespace qpc

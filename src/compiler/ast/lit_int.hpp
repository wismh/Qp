#pragma once

#include <optional>
#include <string>

namespace qpc {

struct LitInt {
    std::string raw;
    std::optional<std::string> suffix;
};

}  // namespace qpc

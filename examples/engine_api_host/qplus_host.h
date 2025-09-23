#pragma once

#include <cstdint>

namespace qplus {

struct World {
    std::int32_t step() const { return 2; }
};

inline World world{};

inline std::int32_t host_bonus() { return 1; }

}  // namespace qplus

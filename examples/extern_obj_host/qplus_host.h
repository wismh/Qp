#pragma once

#include <cstdint>

namespace qplus {

struct Test {
    int c = 3;

    template <typename T>
    T add(T a, T b) {
        return a + b + c;
    }

    static std::int32_t created() {
        return 1;
    }
};

}  // namespace qplus

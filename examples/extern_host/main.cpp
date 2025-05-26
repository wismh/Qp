#include "extern_fn.h"

#include <cstdint>
#include <string>

extern "C" std::int32_t c_mul(std::int32_t a, std::int32_t b) {
    return a * b;
}

namespace qplus {

std::int32_t host_add(std::int32_t a, std::int32_t b) {
    return a + b;
}

String host_greet(String name) {
    return String("hi, ") + name;
}

}  // namespace qplus

int main() {
    const auto c = qplus::via_c(3, 4);
    const auto h = qplus::via_host(2, 5);
    const auto hi = qplus::hello("q");
    const bool ok = c == 12 && h == 7 && hi == "hi, q";
    return ok ? 0 : 1;
}

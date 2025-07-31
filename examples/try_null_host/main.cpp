#include "try_null.h"

int main() {
    std::int32_t x = 1;
    std::int32_t y = 2;
    std::int32_t z = 0;
    std::int32_t* s = qplus::sum(&x, &y);
    std::int32_t* zero = qplus::sum(&z, &z);
    const bool ok = s != nullptr && *s == 1 && qplus::sum(nullptr, &y) == nullptr &&
                    qplus::sum(&x, nullptr) == nullptr && zero == nullptr;
    return ok ? 0 : 1;
}

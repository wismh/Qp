#include "if_let.h"

int main() {
    std::int32_t n = 7;
    std::int32_t m = 3;
    const bool ok = qplus::or_zero(nullptr) == 0 && qplus::or_zero(&n) == 7 &&
                    qplus::both(nullptr, &m) == 0 && qplus::both(&n, nullptr) == 7 &&
                    qplus::both(&n, &m) == 10;
    return ok ? 0 : 1;
}

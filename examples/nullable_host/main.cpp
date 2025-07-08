#include "nullable.h"

int main() {
    std::int32_t n = 7;
    const bool ok = qplus::or_zero(nullptr) == 0 && qplus::or_zero(&n) == 7 && qplus::none() == nullptr &&
                    qplus::is_none(nullptr) && !qplus::is_none(&n);
    return ok ? 0 : 1;
}

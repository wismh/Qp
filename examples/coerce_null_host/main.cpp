#include "coerce_null.h"

int main() {
    std::int32_t seven = 7;
    const bool ok = qplus::run() == 10 && qplus::take(&seven) == 7 && qplus::take(nullptr) == 0 &&
                    qplus::either(true) != nullptr && *qplus::either(true) == 1 &&
                    qplus::either(false) == nullptr;
    return ok ? 0 : 1;
}

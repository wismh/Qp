#include "early_return.h"

int main() {
    const bool ok = qplus::abs(-3) == 3 && qplus::abs(4) == 4 && qplus::sign(-2) == -1 &&
                    qplus::sign(0) == 0 && qplus::sign(9) == 1 &&
                    qplus::clamp_or_zero(5, 0, 10) == 5 && qplus::clamp_or_zero(-1, 0, 10) == 0 &&
                    qplus::clamp_or_zero(11, 0, 10) == 0;
    return ok ? 0 : 1;
}

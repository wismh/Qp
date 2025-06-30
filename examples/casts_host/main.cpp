#include "casts.h"

int main() {
    const bool ok = qplus::widen(3) == 3 && qplus::trunc(2.9f) == 2 && qplus::flag(true) == 1 &&
                    qplus::red_code() == 0 && qplus::run() == 1 + 2 + 1 + 0;
    return ok ? 0 : 1;
}

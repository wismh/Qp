#include "overload.h"

int main() {
    // abs(-3) == 3, bump()+bump(4) => n == 5, total 8
    const bool ok = qplus::run() == 8 && qplus::abs(-2.5f) == 2.5f;
    return ok ? 0 : 1;
}

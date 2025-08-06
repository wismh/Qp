#include "pair.h"

int main() {
    auto p = qplus::make_pair(2, 3);
    const bool ok = qplus::sum_pair(p) == 5 && qplus::bump_a(p) == 3 && p.a == 2;
    return ok ? 0 : 1;
}

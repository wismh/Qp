#include "closures.h"

int main() {
    const bool ok = qplus::apply_add() == 5 && qplus::twice(4) == 8 && qplus::inc_via_apply() == 5 &&
                    qplus::capture(1) == 1 && qplus::immediate() == 3 && qplus::run() == 22;
    return ok ? 0 : 1;
}

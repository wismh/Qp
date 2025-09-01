#include "dict_for.h"

int main() {
    const bool ok = qplus::sum_entries() == 33;
    return ok ? 0 : 1;
}

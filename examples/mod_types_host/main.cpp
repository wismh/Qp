#include "mod_types.h"

int main() {
    const bool ok = qplus::run() == 3;
    return ok ? 0 : 1;
}

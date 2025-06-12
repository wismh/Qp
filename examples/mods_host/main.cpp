#include "mods.h"

int main() {
    const bool ok = qplus::run() == 14;
    return ok ? 0 : 1;
}

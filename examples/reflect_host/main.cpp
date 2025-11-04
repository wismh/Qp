#include "reflect.h"

int main() {
    const bool ok = qplus::run() == 6;
    return ok ? 0 : 1;
}

#include "engine_api.h"

int main() {
    const bool ok = qplus::run() == 3;
    return ok ? 0 : 1;
}

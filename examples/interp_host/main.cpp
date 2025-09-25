#include "interp.h"

int main() {
    const bool ok = qplus::status(10) == "hp = 10" && qplus::greet("Ada") == "hello, Ada" &&
                    qplus::mix(3, true) == "3:true";
    return ok ? 0 : 1;
}

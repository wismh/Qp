#include "to_string.h"

int main() {
    const bool ok = qplus::label(7) == "n=7" && qplus::flag(true) == "true" &&
                    qplus::flag(false) == "false" &&
                    qplus::color_name(qplus::Color::Red) == "0" &&
                    qplus::color_name(qplus::Color::Green) == "1";
    return ok ? 0 : 1;
}

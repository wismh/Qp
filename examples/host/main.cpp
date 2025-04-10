#include "add.h"

#include <iostream>

int main() {
    const auto result = qplus::add(2, 3);
    std::cout << result << '\n';
    return result == 5 ? 0 : 1;
}

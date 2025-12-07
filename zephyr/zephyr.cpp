module;

#include <iostream>

export module zephyr;

namespace zephyr {
export class Zephyr {
public:
    Zephyr() = default;

    auto init() -> void {
        std::cout << "Why Hello!\n";
    }
};
}

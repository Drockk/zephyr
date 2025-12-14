#pragma once

#include "zephyr/io/ioUringContext.hpp"
#include "zephyr/plugins/udp/concept.hpp"

#include <memory>

namespace zephyr::plugins::udp
{
template <int PORT, ControllerConcept Controller>
class Plugin
{
public:
    auto init() {}

    auto start() {}

    auto stop() {}

private:
    Controller m_controller{};
    std::shared_ptr<io::IoUringContext> m_ioContext{std::make_shared<io::IoUringContext>()};
};
}  // namespace zephyr::plugins::udp

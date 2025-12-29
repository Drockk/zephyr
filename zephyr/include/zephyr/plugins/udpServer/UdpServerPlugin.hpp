#pragma once

// #include "zephyr/common/resultSender.hpp"
#include "zephyr/core/logger.hpp"
// #include "zephyr/io/ioUringContext.hpp"
#include "zephyr/network/endpoint.hpp"
#include "zephyr/network/udpSocket.hpp"
#include "zephyr/plugins/udpServer/details/concept.hpp"
#include "zephyr/plugins/udpServer/details/protocol.hpp"

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace zephyr::plugins::udp
{
template <ControllerConcept Controller>
class Plugin
{
public:
    template <typename ControllerArg>
        requires std::constructible_from<Controller, ControllerArg>
    explicit Plugin(network::UdpEndpoint t_endpoint, ControllerArg&& t_controller)
        : m_controller(std::forward<ControllerArg>(t_controller)),
          m_endpoint(t_endpoint)
    {}

    Plugin(const Plugin&) = delete;
    Plugin(Plugin&& t_other) noexcept
        : m_controller(std::move(t_other.m_controller)),
          m_isRunning(t_other.m_isRunning.load()),
          m_logger(std::move(t_other.m_logger)),
          m_endpoint(std::move(t_other.m_endpoint)),
          m_socket(std::move(t_other.m_socket))
    {
        t_other.m_isRunning.store(false);
    }

    Plugin& operator=(const Plugin&) = delete;
    Plugin& operator=(Plugin&&) noexcept = default;

    ~Plugin() = default;

    auto init() -> void
    {
        m_logger = core::Logger::createLogger("UDP");
        m_socket = network::UdpSocket{m_endpoint};
        ZEPHYR_LOG_INFO(m_logger, "Initializing UDP plugin");
        m_socket->bind(m_logger);
    }

    auto start(stdexec::scheduler auto t_scheduler) -> void
    {
        // receiveLoop(t_scheduler);
    }

    auto stop()
    {
        m_isRunning.store(false);
        // m_ioContext->cancel();
        if (m_socket) {
            m_socket->close();
        }
    }

private:
    auto receiveLoop([[maybe_unused]] stdexec::scheduler auto t_scheduler) -> void
    {
        if (m_isRunning.load()) {
            return;
        }

        // auto strandScheduler =

        // auto work = stdexec::schedule(t_scheduler) | stdexec::then([this]() { receive(); })
        //             | stdexec::let_value([this](auto t_maybePacket) {
        //                   using Result = std::optional<std::pair<std::vector<uint8_t>, sockaddr_in>>;
        //                   using Sender = common::ResultSender<Result>;

        //                   if (!t_maybePacket) {
        //                       return Sender{stdexec::just(Result{})};
        //                   }

        //                   auto [packet, address] = std::move(*t_maybePacket);
        //                   return Sender()
        //               });
        //             | stdexec::then(/*maybe response*/)
        //             | stdexec::upon_error([this, scheduler = t_scheduler](std::exception_ptr t_exceptionPtr) {
        //                   try {
        //                       std::rethrow_exception(t_exceptionPtr)
        //                   } catch (const std::exception& t_exception) {
        //                       // Log Error
        //                   }

        //                   if (m_isRunning.load()) {
        //                       receiveLoop(scheduler);
        //                   }
        //               });

        // stdexec::start_detached(std::move(work));
    }

    auto receive() -> std::optional<std::pair<UdpProtocol::InputType, sockaddr_in>>
    {
        // if (!m_isRunning.load()) {
        //     return std::nullopt;
        // }

        // std::array<std::byte, 65536> buffer{};
        // sockaddr_in clientAddress{};

        // auto messageLength = m_ioContext->recvfrom(m_socket, buffer, clientAddress);
        // if (!m_isRunning.load() || messageLength <= 0) {
        //     return std::nullopt;
        // }

        // sockaddr_in localAddress{};
        // socklen_t localAddressLength = sizeof(localAddress);
        // getsockname(m_socket, reinterpret_cast<sockaddr*>(&localAddress), &localAddressLength);

        // UdpProtocol::InputType packet{.sourceIp = inet_ntoa(clientAddress.sin_addr),
        //                               .sourcePort = ntohs(clientAddress.sin_port),
        //                               .destPort = ntohs(localAddress.sin_port),
        //                               .clientAddr = clientAddress,
        //                               .data = {reinterpret_cast<uint8_t*>(buffer.data()),
        //                                        reinterpret_cast<uint8_t*>(buffer.data()) + messageLength}};

        // return std::make_pair(std::move(packet), clientAddress);
        return std::nullopt;
    }

    Controller m_controller{};
    // std::shared_ptr<io::IoUringContext> m_ioContext{std::make_shared<io::IoUringContext>()};
    std::atomic<bool> m_isRunning{false};
    core::Logger::LoggerPtr m_logger;
    network::UdpEndpoint m_endpoint;
    std::optional<network::UdpSocket> m_socket;
};

template <typename ControllerArg>
Plugin(network::UdpEndpoint, ControllerArg&&) -> Plugin<std::remove_cvref_t<ControllerArg>>;

}  // namespace zephyr::plugins::udp

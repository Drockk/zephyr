#pragma once

// #include "zephyr/common/resultSender.hpp"
#include "zephyr/core/logger.hpp"
// #include "zephyr/io/ioUringContext.hpp"
#include "zephyr/plugins/udp/concept.hpp"
#include "zephyr/plugins/udp/details/protocol.hpp"

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
#include <unistd.h>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace zephyr::plugins::udp
{
template <int16_t PORT, ControllerConcept Controller>
class Plugin
{
public:
    Plugin() = default;
    Plugin(const Plugin&) = delete;
    Plugin(Plugin&&) = delete;

    Plugin& operator=(const Plugin&) = delete;
    Plugin& operator=(Plugin&&) = delete;

    ~Plugin()
    {
        stop();
    }

    auto init() -> void
    {
        m_logger = core::Logger::createLogger("UDP");

        ZEPHYR_LOG_INFO(m_logger, "Starting UDP plugin");

        bindSocket();
    }

    auto start(stdexec::scheduler auto t_scheduler) -> void
    {
        receiveLoop(t_scheduler);
    }

    auto stop()
    {
        m_isRunning.store(false);
        // m_ioContext->cancel();
        if (m_socket >= 0) {
            close(m_socket);
            m_socket = -1;
        }
    }

private:
    auto bindSocket() -> void
    {
        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0) {
            throw std::runtime_error(std::format("Create UDP socket"));
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        if (bind(m_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            close(m_socket);
            m_socket = -1;
            throw std::runtime_error("Cannot bind UDP socket to: ADDRES:PORT, error?");
        }

        ZEPHYR_LOG_INFO(m_logger, "Bound {}:{}", INADDR_ANY, PORT);
    }

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
    }

    Controller m_controller{};
    // std::shared_ptr<io::IoUringContext> m_ioContext{std::make_shared<io::IoUringContext>()};
    int m_socket{-1};
    std::atomic<bool> m_isRunning{false};
    core::Logger::LoggerPtr m_logger;
};
}  // namespace zephyr::plugins::udp

#pragma once

#include "zephyr/core/logger.hpp"
#include "zephyr/execution/strandScheduler.hpp"
#include "zephyr/network/endpoint.hpp"
#include "zephyr/network/udpSocket.hpp"
#include "zephyr/plugins/udpServer/details/concept.hpp"
#include "zephyr/plugins/udpServer/details/protocol.hpp"

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <atomic>
#include <concepts>
#include <optional>
#include <unistd.h>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace zephyr::plugins
{
template <udp::ControllerConcept Controller, stdexec::scheduler BaseScheduler>
class UdpServer
{
public:
    template <typename ControllerArg>
        requires std::constructible_from<Controller, ControllerArg>
    explicit UdpServer(network::UdpEndpoint t_endpoint, ControllerArg&& t_controller)
        : m_controller(std::forward<ControllerArg>(t_controller)),
          m_endpoint(t_endpoint)
    {}

    UdpServer(const UdpServer&) = delete;
    UdpServer(UdpServer&& t_other) noexcept
        : m_controller(std::move(t_other.m_controller)),
          m_strand(std::move(t_other.m_strand)),
          m_isRunning(t_other.m_isRunning.load()),
          m_logger(std::move(t_other.m_logger)),
          m_endpoint(std::move(t_other.m_endpoint)),
          m_socket(std::move(t_other.m_socket))
    {
        t_other.m_isRunning.store(false);
    }

    UdpServer& operator=(const UdpServer&) = delete;
    UdpServer& operator=(UdpServer&&) noexcept = default;

    ~UdpServer() = default;

    auto init(stdexec::scheduler auto t_scheduler) -> void
    {
        m_strand = execution::StrandScheduler<BaseScheduler>(std::move(t_scheduler));
        m_logger = core::Logger::createLogger("UDP");
        m_socket = network::UdpSocket{m_endpoint};

        ZEPHYR_LOG_INFO(m_logger, "Initializing UDP plugin");

        m_socket->bind(m_logger);
    }

    auto start() -> void
    {
        // receiveLoop();
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

    auto receive() -> std::optional<std::pair<udp::UdpProtocol::InputType, sockaddr_in>>
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
    std::optional<execution::StrandScheduler<BaseScheduler>> m_strand;
    std::atomic<bool> m_isRunning{false};
    core::Logger::LoggerPtr m_logger;
    network::UdpEndpoint m_endpoint;
    std::optional<network::UdpSocket> m_socket;
};

// Deduction guide for scheduler-agnostic construction
// When UdpServer is created without a scheduler, use io_uring scheduler for type deduction
template <typename ControllerArg>
UdpServer(network::UdpEndpoint, ControllerArg&&) -> UdpServer<std::remove_cvref_t<ControllerArg>, exec::__io_uring::__scheduler>;

}  // namespace zephyr::plugins

#include <exec/single_thread_context.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <zephyr/context/context.hpp>
#include <zephyr/execution/strandScheduler.hpp>
#include <zephyr/http/httpMessages.hpp>
#include <zephyr/http/httpPipelineBuilder.hpp>
#include <zephyr/http/httpRouter.hpp>
#include <zephyr/http/middlewares/loggingMiddleware.hpp>
#include <zephyr/io/ioUringContext.hpp>
#include <zephyr/pipeline/pipelineConcept.hpp>
#include <zephyr/pipelines/rawPipeline.hpp>
#include <zephyr/tcp/tcpProtocol.hpp>
#include <zephyr/tcp/tcpServer.hpp>
#include <zephyr/tcp/tcpSession.hpp>
#include <zephyr/udp/udpProtocol.hpp>
#include <zephyr/udp/udpRouter.hpp>
#include <zephyr/udp/udpRouterPipeline.hpp>
#include <zephyr/udp/udpServer.hpp>

namespace ex = stdexec;
using namespace zephyr::tcp;
using namespace zephyr::udp;

// ============================================================================
// MAIN
// ============================================================================

int main()
{
    exec::static_thread_pool pool(4);
    auto scheduler = pool.get_scheduler();

    // HTTP Router
    zephyr::http::HttpRouter http_router;

    http_router.get("/", [](const auto&, const auto&) { return zephyr::http::HttpResponse::ok("Welcome!"); });

    http_router.get("/users/:id", [](const auto& req, const auto&) {
        auto id = req.path_params.at("id");
        return zephyr::http::HttpResponse::json(R"({"id": ")" + id + R"("})");
    });

    http_router.post("/echo",
                     [](const auto& req, const auto&) { return zephyr::http::HttpResponse::ok("Echo: " + req.body); });

    // UDP Router
    zephyr::udp::UdpRouter udp_router;

    udp_router.on_port(5000, [](const auto& packet, const auto&) {
        std::cout << "[UDP:5000] Echo " << packet.data.size() << " bytes\n";
        return std::optional{packet.data};
    });

    // Create servers
    auto http_io = std::make_shared<zephyr::io::IoUringContext>();
    auto echo_io = std::make_shared<zephyr::io::IoUringContext>();
    auto udp_io = std::make_shared<zephyr::io::IoUringContext>();

    // HTTP Server (TCP + HTTP Pipeline) - using HttpPipelineBuilder with middlewares
    auto http_pipeline_factory =
        zephyr::http::HttpPipelineBuilder<>()
            .with_router(http_router)
            .with_middleware(zephyr::http::logging_middleware())
            .with_middleware(
                [](zephyr::http::HttpRequest req) -> zephyr::common::ResultSender<zephyr::http::HttpRequest> {
                    // CORS middleware - dodaje nagłówki do response przez kontekst
                    std::cout << "[Middleware:CORS] Adding headers for " << req.path << "\n";
                    return zephyr::common::ResultSender<zephyr::http::HttpRequest>{ex::just(std::move(req))};
                })
            .build();

    auto http_server = make_tcp_server(scheduler, http_pipeline_factory, http_io);

    // Echo Server (TCP + Raw Pipeline)
    auto echo_server = make_tcp_server(
        scheduler,
        []() {
            return zephyr::pipelines::make_raw_pipeline<zephyr::tcp::TcpProtocol>(
                [](std::string data) -> zephyr::tcp::TcpProtocol::OutputType { return "ECHO: " + data; });
        },
        echo_io);

    // UDP Server (UDP + Router Pipeline)
    auto udp_server =
        make_udp_server(scheduler, [&udp_router]() { return zephyr::udp::UdpRouterPipeline(udp_router); }, udp_io);

    // Start servers
    if (!http_server.listen(8080) || !echo_server.listen(9000) || !udp_server.bind(5000)) {
        std::cerr << "Failed to start servers\n";
        return 1;
    }

    http_server.run();
    echo_server.run();
    udp_server.run();

    std::cout << "\nServers running:\n"
              << "  HTTP: http://localhost:8080/\n"
              << "  Echo: nc localhost 9000\n"
              << "  UDP:  echo test | nc -u localhost 5000\n\n"
              << "Press Ctrl+C to stop...\n\n";

    std::this_thread::sleep_for(std::chrono::seconds(60));

    std::cout << "\nStopping servers...\n";
    http_server.stop();
    echo_server.stop();
    udp_server.stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.request_stop();

    std::cout << "Done.\n";
    return 0;
}

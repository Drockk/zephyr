#include <format>
#include <iostream>
#include <memory>
#include <thread>
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

import zephyr.io.ioUringContext;

int main()
{
    std::cout << "=== TCP Server with io_uring and lambda data handler ====\n\n";

    try {
        auto io_context = std::make_shared<zephyr::io::IoUringContext>();

        std::jthread io_thread([io_context]() {
            io_context->run();
        });

        exec::static_thread_pool thread_pool{ 4 };
        // auto accept_strand = StrandScheduler{};

        auto my_handler = [](std::string& t_data) {
            std::cout << std::format("\nLAMBDA HANDLER CALLED\n");
            std::cout << std::format("Data received ({} bytes): {}\n", t_data.size(), t_data);

            if (t_data.find("\n") != std::string::npos) {
                std::cout << "Complete request - generating response\n";

                auto response = "Processed " + std::to_string(t_data.size()) + " bytes\n";
                return response;
            }

            std::cout << "Partial request - waiting for more data\n";
            return std::string{};
        };

        // TcpServer server(pool, accept_strand, io_context, my_handler);

        // if (server.listen(8080)) {
            std::cout << "\nServer is running!\n";
            std::cout << "You can connect using: telnet localhost 8080\n";
            std::cout << "Or: echo 'test' | nc localhost 8080\n\n";

        //     server.run();

            std::this_thread::sleep_for(std::chrono::seconds(60));

            std::cout << "\nStopping...\n";
        //     server.stop();
            io_context->stop();
        // }
    }
    catch (const std::exception& t_exception) {
        std::cerr << std::format("Exception: {}\n", t_exception.what());
        return 1;
    }

    return 0;
}
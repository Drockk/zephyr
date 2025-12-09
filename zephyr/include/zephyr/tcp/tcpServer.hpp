#pragma once

#include <memory>
#include <functional>

namespace zephyr::tcp
{
class TcpServer
{
public:
    TcpServer(exec::static_thread_pool& pool, StrandScheduler strand,
        std::shared_ptr<IoUringContext> ctx,
        std::function<std::string(const std::string&)> handler)
            : accept_strand(strand), thread_pool(pool), io_ctx(ctx),
                data_handler(std::move(handler)) {}

    ~TcpServer()
    {
        stop();
    }

    bool listen(int port)
    {
        listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket < 0) {
            return false;
        }
    
        int opt = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
    
        if (bind(listen_socket, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(listen_socket);
            return false;
        }
    
        if (::listen(listen_socket, 128) < 0) {
            close(listen_socket);
            return false;
        }

        return true;
    }

    void run()
    {
        accept_loop();
    }

private:
    void accept_loop()
    {
        auto work =
            ex::schedule(accept_strand)

            // Accept przez io_uring - całkowicie asynchroniczny!
        | ex::let_value([this]() {
            std::cout << "Rejestruję accept w io_uring...\n";
            return IoUringAcceptSender(io_ctx, listen_socket);
        })
        
        // Utwórz sesję dla nowego połączenia
        | ex::then([this](int client_socket) {
            std::cout << "Nowe połączenie: fd=" << client_socket << "\n";
            
            auto session_strand = make_strand_scheduler(thread_pool);
            
            // Tworzymy sesję PRZEKAZUJĄC LAMBDĘ użytkownika
            auto session = std::make_shared<Session>(
                client_socket,
                session_strand,
                io_ctx,
                data_handler  // Lambda od użytkownika trafia tutaj!
            );
            
            active_sessions[client_socket] = session;
            session->start();
            
            if (is_running) {
                accept_loop();
            }
        })
        
        | ex::upon_error([this](std::exception_ptr eptr) {
            try {
                std::rethrow_exception(eptr);
            } catch (const std::exception& e) {
                std::cerr << "Błąd accept: " << e.what() << "\n";
            }
            if (is_running) {
                accept_loop();
            }
        });
    
    ex::start_detached(std::move(work));
}

StrandScheduler make_strand_scheduler(exec::static_thread_pool& pool) {
    return StrandScheduler{};
}
```

public:
void stop() {
std::cout << “Zatrzymywanie serwera…\n”;
is_running = false;

```
    for (auto& [fd, session] : active_sessions) {
        session->stop();
    }
    active_sessions.clear();
    
    if (listen_socket >= 0) {
        close(listen_socket);
        listen_socket = -1;
    }
}

int listen_socket = -1;
StrandScheduler accept_strand;
exec::static_thread_pool& thread_pool;
std::shared_ptr<IoUringContext> io_ctx;

std::function<std::string(const std::string&)> data_handler;

std::map<int, std::shared_ptr<Session>> active_sessions;
bool is_running = true;
};
}
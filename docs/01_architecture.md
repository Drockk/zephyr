# 1. Framework Architecture

Zephyr consists of the following layers:

`Client -> ProtocolPlugin -> InternalMessage -> Router -> ControllerStrand -> Handler -> Response`

SvarogIO is responsible for I/O and event loops.

## 1.1. Components

- **Application** - plugin composition
- **Router** - compile-time route mapping -> handler
- **ControllerStrand** - single-thread context for handlers
- **InternalMessage** - universal request format
- **HttpResponse** - MVP response (HTML only)

## 1.2. Flow

`socket_accept -> HTTP decode -> InternalMessage -> Router -> ControllerStrand -> Handler -> HttpResponse -> encode -> write`

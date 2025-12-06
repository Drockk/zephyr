# 1. Architecture

Zephyr consists of several cooperating layers. Networking and event loop are provided by SvarogIO. Zephyr adds routing, controllers, plugin integration and application tooling.

## 1.1 High-level flow

`Client -> ProtocolPlugin -> InternalMessage -> Router -> ControllerStrand -> Handler -> Response -> Protocol encode -> Write`

## 1.2 Main components

| Component | Responsibility |
|---|---|
| Application | Composition of plugins and services |
| Plugin | Adds a feature/protocol (HTTP, DB, Metrics...) |
| Router | Compile-time mapping of request → handler |
| Controller | User logic and route definitions |
| ControllerStrand | Executes handlers without multi-thread concerns |
| InternalMessage | Common internal request format |
| Response | Handler output model (MVP: HTML only) |

## 1.3 Why this architecture?

- Predictable performance (no dynamic dispatch)
- Easy to extend via plugins
- Handlers run single-thread → no mutex headaches
- Future-proof for multi-protocol routing

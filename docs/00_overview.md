# Zephyr - Overview

Zephyr is a modular C++23 framework based on SvarogIO. The project aims to enable services with an architecture similar to Spring Boot/Axum, while maintaining low runtime overhead, compile-time configuration, and a modern API.

Key features:

- Modularity through plugins (HTTP, UDP, DB, Metrics...)
- Compile-time routing, minimal macros
- No runtime exceptions -> `std::expected`
- Strands model -> user code written as single-threaded
- Cross-protocol message bus in the future

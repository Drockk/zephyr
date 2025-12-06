# Zephyr — Overview

Zephyr is a modular backend framework written in modern C++23. It aims to provide an experience similar to Spring Boot or Rust Axum, while keeping performance close to raw asynchronous networking libraries.

The framework is built on top of **SvarogIO** — an asynchronous runtime and networking layer similar to Boost.Asio. Zephyr focuses on high-level server framework capabilities.

## Core goals

- Modular architecture — features shipped as independent plugins
- Compile-time routing (no runtime dispatch cost)
- Minimal runtime overhead, no exceptions → `std::expected`
- Request handlers run inside **Strands** → single-thread logic, no locks
- Extensible messaging model — support for multiple protocols
- HTTP first in MVP, UDP/WebSocket/DB plugins planned

## Vision

- Small footprint like µWebSockets.
- Productivity like Spring Boot.
- Compile-time guarantees like Rust.

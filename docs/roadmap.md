# Zephyr Roadmap

Roadmap tracks planned evolution of the framework.
Milestones are intentionally incremental — first we deliver a minimal but
clean, fast and well-architected HTTP framework, then grow functionality in layers.

---

## v0.1 — HTTP MVP (Hello World)

**Goal:** serve HTTP GET "/" using compile-time routing.

Includes:

- Core Application<> + Plugin system (compile-time)
- InternalMessage base model
- Compile-time RouterEngine + route tables
- HTTP plugin (decoder + encoder)
- ROUTES/GET/POST DSL for controllers
- ControllerStrand executor
- Example app + E2E test
- No headers/JSON yet — **HTML only**

Why this version matters:  
Foundation of the whole framework. From this point forward,
every next feature extends a working base.

---

## v0.2 — Response features + JSON support

**Goal:** make Zephyr usable for simple APIs, not just HTML pages.

Planned additions:

- HttpResponse builders: `.json()`, `.text()`, `.binary()`
- Status codes as API (`.status(201)`)
- Basic HTTP headers
- Internal error details (message, description, reason)
- Simple request body access

Why:  
Most service scenarios need JSON and headers. This makes Zephyr useful for small REST services.

---

## v0.3 — Middleware & Pipeline System

**Goal:** add request/response processing steps.

Features:

- Pipeline design (pre-routing + post-routing hooks)
- Authentication middleware example
- Logging middleware adapter
- Compile-time middleware composition if feasible

Why:  
Brings Zephyr closer to real backend usability (CORS, auth, logging).

---

## v0.4 — UDP Plugin

**Goal:** first multi-protocol expansion.

Features:

- UDP listener plugin based on SvarogIO
- Basic message passthrough to RouterEngine using custom `UdpRouteKey`
- Minimal routing syntax for UDP (TBD — e.g. `ON_PACKET(...)`)

Why:  
Tests our architecture abstraction — routing must stay shared across protocols.

---

## v0.5 — Metrics + Logging

**Goal:** observability and production readiness.

Features:

- Metrics plugin compatible with Prometheus exporter model
- Counter, gauge API
- Logging plugin (spdlog backend by default)

Why:  
Essential for monitoring and debugging real deployments.

---

## v0.6 — Database Layer (Local first)

**Goal:** allow storing/querying application data.

Plan (incremental delivery):

1. SQLite plugin (local testing, embedded apps)
2. PostgreSQL plugin (production use)
3. Async DB interface concept using strands

Why:  
Databases unlock real applications beyond stateless microservices.

---

## v0.7 — WebSocket & Streaming

**Goal:** bidirectional, long-lived connections.

Features:

- WS handshake + frame encode/decode
- Per-connection state handled inside strands
- Broadcast / subscription API

Why:  
Enables chat/streaming/dashboard applications.

---

## v0.8 — gRPC / Protobuf Integration

**Goal:** structured message RPC communication.

Features:

- Proto codegen integration (plugin or external tool)
- Unary requests first; streaming later
- Map protobuf services to routes compile-time

Why:  
gRPC unlocks inter-service communication and microservice workloads.

---

## v0.9 — Cross-protocol routing / Bridges

**Goal:** allow routing messages between protocols.

Examples:

- UDP → trigger HTTP handler
- WebSocket → serve controller handler
- Internal events to pipeline

Why:  
Multi-protocol orchestration = advanced use case where Zephyr can shine.

---

## v1.0 — Stable Core Release

**Goal:** production-ready release with strong guarantees.

Requirements:

- Performance tuning & benchmarks
- Stable public API
- Plugins versioning and compatibility policy
- Documentation site + examples library

Why:  
1.0 means we commit to API stability and expect community usage.

---

## Future Ideas (Parked)

Not scheduled but aligned with vision:

- Hot-reload config or partial recompilation support
- HTTP/2, HTTP/3, QUIC plugin
- WASM controllers execution
- Plugin marketplace registry
- Live application introspection dashboard
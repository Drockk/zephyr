# 4. Protocol plugins

## 4.1 HTTP (MVP)

Features included:

- TCP listener via SvarogIO
- Request decode
- Routing -> Handler
- HTML Response encode

Example:

```cpp
using App = Application<Http<8080>, Controller<Hello>>;
App{}.start();
```

### Future plugins

| Plugin | Purpose |
|--------|---------|
| UDP | Lightweight message handling |
| WebSocket | Real-time communication |
| gRPC | RPC communication |
| DB(Postgre/SQLite/Redis) | Data access via plugins |
| Metrics/Prometheus | Observability |

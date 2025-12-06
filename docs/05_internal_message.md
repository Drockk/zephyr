# 5. InternalMessage

`InternalMessage` is the unified structure that represents a request
inside the framework, after protocol parsing.

```cpp
struct InternalMessage {
    std::string_view route;               // "/api/test"
    std::span<const std::byte> body;      // raw request data
    ProtocolId protocol;                  // HTTP, UDP...
    ConnectionId connection;              // responder context

    std::string_view text() const noexcept;
};
```

Future fields to consider:

- headers
- method
- query params

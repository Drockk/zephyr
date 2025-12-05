# 5.InternalMessage

```cpp
struct InternalMessage { 
std::string_view route; 
std::span<const std::byte> body; 
ProtocolId protocol; 
ConnectionId connection; 

std::string_view text() const noexcept;
};
```

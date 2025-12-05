# 6. HttpResponse & Error Handling

```cpp
enum class ErrorCode { NotFound, BadRequest, InternalError };
```

In MVP, we only support HTML.

```cpp
std::expected<HttpResponse, ErrorCode>
handler(const InternalMessage&);
```

404/400/500 mapped automatically.
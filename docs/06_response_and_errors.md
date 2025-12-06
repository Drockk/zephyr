# 6. Response & Error handling

## 6.1 HttpResponse (MVP)

```cpp
struct HttpResponse {
    std::string body;

    static HttpResponse html(std::string_view text) {
        return HttpResponse{std::string{text}};
    }
};
```

## 6.2 Handler return type

```cpp
std::expected<HttpResponse, ErrorCode>
handler(const InternalMessage&);
```

## 6.3 Error mapping

| Error | HTTP |
|-------|------|
| NotFound | 404 |
| BadRequest | 400 |
| InternalError | 500 |

Future:
- JSON, headers, custom codes

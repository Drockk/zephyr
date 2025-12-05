# 3. Routing

Routing is compile-time defined by the macro:

```cpp
struct HelloController {
    ROUTES(
        GET("/", { return HttpResponse::html("Hello"); })
    )
};
```

Lookup does not create allocations and runs in O(1).

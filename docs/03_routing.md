# 3. Routing

Routing is resolved at compile-time. No dynamic maps, no strings comparisons in runtime.

## 3.1 User example

```cpp
struct Hello {
    ROUTES(
        GET("/", { return HttpResponse::html("Hello"); })
    )
};
```

## 3.2 How routing works

`compile-time macros -> route table constexpr -> lookup -> handler pointer -> execute`

### MVP only supports

- GET, POST
- Path matching /example
- No path params or wildcards yet

### Future features

- Path_params /user/{id}
- Middlewares
- Multi-protocol routing (UDP -> Controller)

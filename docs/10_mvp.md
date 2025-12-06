# MVP Roadmap

Goal: Serve HTTP request and reply "Hello".

## Required

- Core Application type
- Plugin loader system
- HTTP Listener + decoder
- InternalMessage model
- Router (GET/POST)
- Controller macros
- HttpResponse::html
- Response encoding

## Result app

```cpp
struct Hello {
    ROUTES(GET("/", { return HttpResponse::html("Hello World"); }));
};

using App = Application<Http<8080>, Controller<Hello>>;
App{}.start();
```

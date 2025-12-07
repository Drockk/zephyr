# Zephyr — MVP Milestone v0.1

Goal: Serve HTTP GET "/" -> "Hello World" using compile-time routing and HTTP plugin.

Framework must build, start, accept connection, route request, execute handler
in a strand, return HTML response, and shut down cleanly.

No dynamic plugin loading. All routing and plugin activation is compile-time
through template composition.

---

## Milestone summary

| Phase | Scope | Status |
|---|---|---|
| 1. Core Foundation | Application, Plugin base, project skeleton | ☐ |
| 2. Compile-Time Routing | Route table, DSL macros, concat routes | ☐ |
| 3. HTTP Plugin | Decoder/Encoder + table build | ☐ |
| 4. Execution Path | Strands + dispatch pipeline | ☐ |
| 5. Finalization | Example, tests, docs & release | ☐ |

---

## Issues

### 1. Repository & Build Setup

**Description:**  
Create initial project structure and build system.

**Tasks:**  

- [+] Create folder layout:

    ```sh
    /src
    /include/zephyr
    /docs
    /examples
    ```

- [+] Add `CMakeLists.txt` (minimal library target)
- [ ] Add `.clang-format` (optional)
- [+] Add license file (MIT recommended)

**Definition of Done:**  
Running `cmake . && make` builds empty project target.

---

### 2. Implement `Application<>` type

**Description:**  
Basic application container which stores plugin pack.

Requirements:

- [+] `template<class... Plugins> struct Application { ... };`
- [+] `.start()` calls plugin lifecycle hooks (empty for now)
- [+] Construct-only compile-time plugin registration

**DoD:**  

```cpp
Application<> app;
app.start(); // compiles & runs no-op
```

### 3. Plugin system — compile-time registration

**Description:**  
Define plugin contract and static integration with Application.

**Tasks:**  

- [+] Create `PluginConcept` or CRTP base
- [+] `init()` and `start()` lifecycle hooks
- [+] Plugins passed as template parameters

**DoD:**  
Compile with dummy plugin:

```cpp
Application<DummyPlugin> app;
app.start(); // calls DummyPlugin::init (empty)
```

### 4. `InternalMessage` core struct

**Description:**  
Unified message format used inside framework.

Fields MVP:

- [ ] method enum or protocol-specific id
- [ ] path/view
- [ ] body as span/bytes
- [ ] connection/token ID

**DoD:** usable in router & http decoder.

### 5. `RouterEngine` — compile-time table backend

**Description:**  
Central static router engine storing route tables.

**Tasks:**  

- [ ] `setRouteTable(constexpr RouteTable*)`
- [ ] `lookup(RouteKey) -> HandlerFn*`
- [ ] Must be zero dynamic allocation

**DoD:**

```cpp
RouterEngine eng;
eng.setRouteTable(&static_table);
eng.lookup(key);
```

### 6. `RouteKeyConcept` + `RouteEntry`

**Description:**  
Generic abstraction for any protocol key.

**Tasks:**  

- [ ] `RouteKeyConcept` (struct or concept)
- [ ] `RouteEntry<RouteKey, HandlerFn>` POD
- [ ] No references, no dynamic memory

### 7. Compile-time routes concat helper

**Description:**  
Concatenate tuples of routes from controllers.

**Tasks:**  

- [ ] `template<class... Controllers> constexpr auto concat_routes();`
- [ ] Produces `constexpr std::array<RouteEntry,...>`

**DoD:**  
Compile-time evaluated, no runtime loops.

### 8. ROUTES() + GET/POST macros (HTTP DSL)

**Description:**  
Users define routes in controllers using macro DSL.

**Tasks:**  

- [ ] `#define ROUTES(...)`
- [ ] `#define GET(path, handler)`
- [ ] `#define POST(path, handler)`
- [ ] Produces constexpr tuples

**DoD:**

```cpp
struct Hello {
  ROUTES( GET("/", { return HttpResponse::html("Hi"); }) )
};
```

### 9. HTTP RouteKey + compile-time route table build

**Description:**  
RouteKey struct for HTTP.

**Tasks:**  

- [ ] enum class HttpMethod { GET, POST };
- [ ] struct HttpRouteKey { HttpMethod, std::string_view path };
- [ ] Generate constexpr route table from controllers pack
- [ ] Register in RouterEngine using pointer install

**DoD:**  
Router has working static table without runtime registration loop.

### 10. HTTP Request Decoder MVP

**Description:**  
Parse minimal HTTP GET request.

**Tasks:**  

- [ ] Extract method & path
- [ ] Extract body if present
- [ ] No headers parsing in MVP

**DoD:**  
"GET / HTTP/1.1" -> InternalMessage{path="/"}

### 11. HTTP Response Encoder MVP

**Description:**  
Generate minimal HTTP/1.1 response.

**Tasks:**  

- [ ] 200 OK success
- [ ] 404 NotFound
- [ ] 500 InternalError

**Output:**  

```php-template
HTTP/1.1 <code>\r\n
Content-Length: <len>\r\n
\r\n
<body>
```

### 12. ControllerStrand executor

**Description:**  
Single-thread sequential execution for handlers.

**Tasks:**  

- [ ] push/queue handlers inside strand
- [ ] no locks required
- [ ] integrate into handler execution flow

### 13. End-to-end pipeline integration

**Description:**  
Connect everything:

```sh
TCP -> Decode -> RouterEngine lookup -> Handler -> Response -> Encode -> TCP write
```

**Tasks:**  

- [ ] Works for GET "/"
- [ ] ErrorCode maps to response properly

**DoD:**  
`curl localhost:8080` returns `<h1>Hello World</h1>`.

### 14. `/examples/hello_world`

**Description:**  
Provide minimal runnable example.

**Tasks:**  

- [ ] controller + ROUTES
- [ ] main application file
- [ ] README run instructions

### 15. Tests + Release

**Description:**  
MVP must pass tests.

**Tasks:**  

- [ ] Unit tests: router, response builder
- [ ] Integration test: end-to-end HTTP
- [ ] Tag release v0.1

### Completion criteria

Milestone v0.1 is complete when:

- [ ] Project builds cleanly
- [ ] GET "/" returns HTML via curl
- [ ] No dynamic routing, all compile-time
- [ ] No heap usage in routing
- [ ] Docs updated
- [ ] Tagged release

# 2. Components

## Application

Root of user program. Declares active plugins.

```cpp
using App = Application<Http<8080>, Controller<Hello>>;
App{}.start();
```

## Plugin

Extends framework functionality.
Example plugins: HTTP, Logging, Metrics, Database.

## Router

Static lookup table generated at compile time from controller declarations.

## ControllerStrand

Executes handler callbacks sequentially — no shared state issues.

## Controller

User-defined class with ROUTES(...) section.

## InternalMessage

Transport format between protocol decoders and routing layer.

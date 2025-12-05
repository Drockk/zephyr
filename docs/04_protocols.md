# 4. Protocol Plugins

The HTTP plugin is the only one required in MVP.

Usage example:

```cpp
using App = Application<Controller<Hello>, Http<8080>>;
App{}.start();
```

UDP, WebSocket, gRPC - in the roadmap.

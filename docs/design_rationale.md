# Design Rationale

Why compile-time routing?

- zero overhead per request
- easy dead-route detection

Why strands?

- handler code feels single-threaded
- no locking overhead

Why expected instead of exceptions?

- predictable failure path
- consistent with networking style libs

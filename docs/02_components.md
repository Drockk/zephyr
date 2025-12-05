# 2. Components and Responsibilities

| Component | Role |
|-----------|------|
| Application | creates a framework of plugins |
| Plugin | extends functionality (HTTP, DB, Metrics) |
| Controller | user logic + ROUTES |
| ControllerStrand | isolation, no need for mutexes |
| Router | compile-time request dispatch |
| InternalMessage | data transport between layers |

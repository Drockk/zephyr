# MVP Implementation Guide

The goal is to run:

GET "/" → HTML "Hello World"

MVP tasks:

- Application<> + Plugin Loader
- Router (GET/POST)
- HTTP Plugin (listener + decode + encode)
- ControllerStrand
- HttpResponse::html

Shortcomings:

- no middleware
- JSON missing
- no DB

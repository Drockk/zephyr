FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends           \
        cmake                                               \
        clang-format                                        \
        clang-tools-18                                      \
        clangd                                              \
    && apt-get clean                                        \
    && rm -rf /var/lib/apt/lists/*

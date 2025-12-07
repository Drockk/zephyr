add_library(zephyr)

target_sources(zephyr
    PUBLIC
        FILE_SET CXX_MODULES FILES
        core/application.cpp
        core/plugin.cpp
        plugins/dummyPlugin.cpp
        zephyr.cpp
)

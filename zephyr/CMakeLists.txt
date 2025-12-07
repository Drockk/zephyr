add_library(zephyr)

target_sources(zephyr
    PUBLIC
        FILE_SET CXX_MODULES FILES
        zephyr.cpp
)

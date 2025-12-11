# =============================================================================
# Zephyr Library
# =============================================================================
add_library(zephyr SHARED)
add_library(zephyr::zephyr ALIAS zephyr)

# =============================================================================
# Source Files
# =============================================================================
target_sources(zephyr
    PUBLIC
        FILE_SET HEADERS FILES
        "include/zephyr/common/anySender.hpp"
        "include/zephyr/context/context.hpp"
        "include/zephyr/core/application.hpp"
        "include/zephyr/core/plugin.hpp"
        "include/zephyr/execution/strandOp.hpp"
        "include/zephyr/execution/strandState.hpp"
        "include/zephyr/http/middlewares/authMiddleware.hpp"
        "include/zephyr/http/middlewares/httpMiddlewaresConcept.hpp"
        "include/zephyr/http/middlewares/loggingMiddleware.hpp"
        "include/zephyr/http/httpMessages.hpp"
        "include/zephyr/http/httpParser.hpp"
        "include/zephyr/http/httpPipeline.hpp"
        "include/zephyr/http/httpRoute.hpp"
        "include/zephyr/http/httpRouter.hpp"
        "include/zephyr/http/httpSerializer.hpp"
        "include/zephyr/io/ioUringContext.hpp"
        "include/zephyr/pipeline/pipelineConcept.hpp"
        "include/zephyr/pipelines/rawPipeline.hpp"
        "include/zephyr/plugins/dummyPlugin.hpp"
        "include/zephyr/tcp/tcpProtocol.hpp"
        "include/zephyr/udp/udpPacket.hpp"
        "include/zephyr/udp/udpProtocol.hpp"
    PRIVATE
        "source/http/httpMessages.cpp"
        "source/http/httpParser.cpp"
        "source/http/httpPipeline.cpp"
        "source/http/httpRoute.cpp"
        "source/http/httpRouter.cpp"
        "source/http/httpSerializer.cpp"
        "source/io/ioUringContext.cpp"
)

target_link_libraries(zephyr
    PUBLIC
        STDEXEC::stdexec
        ${LIBURING_LIBRARIES}
)

target_include_directories(zephyr
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBURING_INCLUDE_DIRS}
)

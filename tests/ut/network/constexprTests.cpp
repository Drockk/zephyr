#include <catch2/catch_test_macros.hpp>
#include <netinet/in.h>
#include <zephyr/network/addressV4.hpp>
#include <zephyr/network/addressV6.hpp>
#include <zephyr/network/endpoint.hpp>

namespace
{
using namespace zephyr::network;
}

TEST_CASE("Compile-time guarantees", "[constexpr]")
{
    SECTION("AddressV4 constexpr")
    {
        constexpr AddressV4 addr = AddressV4::loopback();
        constexpr bool is_lb = addr.isLoopback();
        static_assert(is_lb);

        constexpr auto uint_val = addr.toUint();
        static_assert(uint_val == 0x7F000001);
    }

    SECTION("AddressV6 constexpr")
    {
        constexpr AddressV6 addr = AddressV6::loopback();
        constexpr bool is_lb = addr.isLoopback();
        static_assert(is_lb);

        constexpr auto bytes = addr.toBytes();
        static_assert(bytes[15] == 1);
    }

    SECTION("endpoint constexpr")
    {
        constexpr TcpEndpoint ep{AddressV4::loopback(), 8080};
        constexpr bool is_v4 = ep.isV4();
        constexpr uint16_t port = ep.port();

        static_assert(is_v4);
        static_assert(port == 8080);
        static_assert(ep.isTcp());
        static_assert(!ep.isUdp());
    }

    SECTION("Protocol type checking")
    {
        static_assert(TcpEndpoint::isTcp());
        static_assert(!TcpEndpoint::isUdp());
        static_assert(UdpEndpoint::isUdp());
        static_assert(!UdpEndpoint::isTcp());

        static_assert(TcpEndpoint::socketType() == SOCK_STREAM);
        static_assert(UdpEndpoint::socketType() == SOCK_DGRAM);
    }
}

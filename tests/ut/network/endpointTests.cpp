#include <catch2/catch_test_macros.hpp>
#include <netinet/in.h>
#include <zephyr/network/addressV4.hpp>
#include <zephyr/network/addressV6.hpp>
#include <zephyr/network/endpoint.hpp>

namespace
{
using namespace zephyr::network;
}

TEST_CASE("endpoint - TCP Construction", "[endpoint][tcp][construction]")
{
    SECTION("From IPv4")
    {
        constexpr TcpEndpoint ep{AddressV4::loopback(), 8080};

        REQUIRE(ep.isV4());
        REQUIRE_FALSE(ep.isV6());
        REQUIRE(ep.port() == 8080);
        REQUIRE(ep.addressV4().isLoopback());
    }

    SECTION("From IPv6")
    {
        constexpr TcpEndpoint ep{AddressV6::loopback(), 8080};

        REQUIRE(ep.isV6());
        REQUIRE_FALSE(ep.isV4());
        REQUIRE(ep.port() == 8080);
        REQUIRE(ep.addressV6().isLoopback());
    }

    SECTION("Factory methods")
    {
        constexpr auto ep1 = TcpEndpoint::fromV4(AddressV4::loopback(), 80);
        constexpr auto ep2 = TcpEndpoint::fromV6(AddressV6::loopback(), 443);

        REQUIRE(ep1.isV4());
        REQUIRE(ep2.isV6());
    }
}

TEST_CASE("endpoint - UDP Construction", "[endpoint][udp][construction]")
{
    SECTION("From IPv4")
    {
        constexpr UdpEndpoint ep{AddressV4::any(), 53};

        REQUIRE(ep.isV4());
        REQUIRE(ep.port() == 53);
    }

    SECTION("From IPv6")
    {
        constexpr UdpEndpoint ep{AddressV6::any(), 53};

        REQUIRE(ep.isV6());
        REQUIRE(ep.port() == 53);
    }
}

TEST_CASE("endpoint - Protocol type info", "[endpoint][protocol]")
{
    SECTION("TCP endpoint")
    {
        constexpr TcpEndpoint ep{AddressV4::loopback(), 8080};

        REQUIRE(ep.isTcp());
        REQUIRE_FALSE(ep.isUdp());
        REQUIRE(TcpEndpoint::socketType() == SOCK_STREAM);
        REQUIRE(TcpEndpoint::protocol() == IPPROTO_TCP);
    }

    SECTION("UDP endpoint")
    {
        constexpr UdpEndpoint ep{AddressV4::loopback(), 8080};

        REQUIRE(ep.isUdp());
        REQUIRE_FALSE(ep.isTcp());
        REQUIRE(UdpEndpoint::socketType() == SOCK_DGRAM);
        REQUIRE(UdpEndpoint::protocol() == IPPROTO_UDP);
    }
}

TEST_CASE("endpoint - Sockaddr conversion", "[endpoint][sockaddr]")
{
    SECTION("IPv4 to sockaddr")
    {
        TcpEndpoint ep{AddressV4{AddressV4::BytesType{192, 168, 1, 1}}, 8080};

        auto [storage, len] = ep.toSockaddr();

        REQUIRE(len == sizeof(sockaddr_in));

        auto* addr = reinterpret_cast<sockaddr_in*>(&storage);
        REQUIRE(addr->sin_family == AF_INET);
        REQUIRE(ntohs(addr->sin_port) == 8080);
        REQUIRE(ntohl(addr->sin_addr.s_addr) == 0xC0A80101);
    }

    SECTION("IPv6 to sockaddr")
    {
        TcpEndpoint ep{AddressV6::loopback(), 443};

        auto [storage, len] = ep.toSockaddr();

        REQUIRE(len == sizeof(sockaddr_in6));

        auto* addr = reinterpret_cast<sockaddr_in6*>(&storage);
        REQUIRE(addr->sin6_family == AF_INET6);
        REQUIRE(ntohs(addr->sin6_port) == 443);
        REQUIRE(addr->sin6_addr.s6_addr[15] == 1);
    }

    SECTION("From sockaddr IPv4")
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8080);
        addr.sin_addr.s_addr = htonl(0x7F000001);  // 127.0.0.1

        TcpEndpoint ep{reinterpret_cast<sockaddr*>(&addr), sizeof(addr)};

        REQUIRE(ep.isV4());
        REQUIRE(ep.port() == 8080);
        REQUIRE(ep.addressV4().isLoopback());
    }

    SECTION("From sockaddr IPv6")
    {
        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(443);
        addr.sin6_addr.s6_addr[15] = 1;  // ::1
        addr.sin6_scope_id = 5;

        TcpEndpoint ep{reinterpret_cast<sockaddr*>(&addr), sizeof(addr)};

        REQUIRE(ep.isV6());
        REQUIRE(ep.port() == 443);
        REQUIRE(ep.addressV6().isLoopback());
        REQUIRE(ep.addressV6().scopeId() == 5);
    }
}

TEST_CASE("endpoint - String parsing", "[endpoint][parsing]")
{
    SECTION("Valid IPv4 endpoints")
    {
        auto ep1 = TcpEndpoint::fromString("192.168.1.1:8080");
        REQUIRE(ep1.has_value());
        REQUIRE(ep1->isV4());
        REQUIRE(ep1->port() == 8080);

        auto ep2 = UdpEndpoint::fromString("127.0.0.1:53");
        REQUIRE(ep2.has_value());
        REQUIRE(ep2->addressV4().isLoopback());
        REQUIRE(ep2->port() == 53);

        auto ep3 = TcpEndpoint::fromString("0.0.0.0:0");
        REQUIRE(ep3.has_value());
        REQUIRE(ep3->port() == 0);
    }

    SECTION("Valid IPv6 endpoints")
    {
        auto ep1 = TcpEndpoint::fromString("[::1]:8080");
        REQUIRE(ep1.has_value());
        REQUIRE(ep1->isV6());
        REQUIRE(ep1->addressV6().isLoopback());
        REQUIRE(ep1->port() == 8080);

        auto ep2 = UdpEndpoint::fromString("[2001:db8::1]:443");
        REQUIRE(ep2.has_value());
        REQUIRE(ep2->isV6());
        REQUIRE(ep2->port() == 443);

        auto ep3 = TcpEndpoint::fromString("[::]:0");
        REQUIRE(ep3.has_value());
        REQUIRE(ep3->addressV6().isUnspecified());
    }

    SECTION("Invalid endpoints")
    {
        REQUIRE_FALSE(TcpEndpoint::fromString("").has_value());
        REQUIRE_FALSE(TcpEndpoint::fromString("192.168.1.1").has_value());  // Missing port
        REQUIRE_FALSE(TcpEndpoint::fromString("192.168.1.1:").has_value());
        REQUIRE_FALSE(TcpEndpoint::fromString("[::1]").has_value());              // Missing port
        REQUIRE_FALSE(TcpEndpoint::fromString("[::1:8080").has_value());          // Missing ]
        REQUIRE_FALSE(TcpEndpoint::fromString("192.168.1.1:99999").has_value());  // Port out of range
        REQUIRE_FALSE(TcpEndpoint::fromString("192.168.1.1:-1").has_value());
    }
}

TEST_CASE("endpoint - String conversion", "[endpoint][string]")
{
    SECTION("IPv4 toString()")
    {
        auto ep = TcpEndpoint::fromString("192.168.1.1:8080");
        REQUIRE(ep->toString() == "192.168.1.1:8080");
    }

    SECTION("IPv6 to_string()")
    {
        auto ep = TcpEndpoint::fromString("[::1]:443");
        REQUIRE(ep->toString() == "[::1]:443");
    }

    SECTION("Roundtrip")
    {
        auto ep1 = TcpEndpoint::fromString("127.0.0.1:8080");
        auto str = ep1->toString();
        auto ep2 = TcpEndpoint::fromString(str);
        REQUIRE(ep2.has_value());
        REQUIRE(*ep1 == *ep2);
    }
}

TEST_CASE("endpoint - Port modification", "[endpoint][port]")
{
    TcpEndpoint ep{AddressV4::loopback(), 8080};

    REQUIRE(ep.port() == 8080);

    ep.port(9090);
    REQUIRE(ep.port() == 9090);
}

TEST_CASE("endpoint - Comparison", "[endpoint][comparison]")
{
    constexpr TcpEndpoint ep1{AddressV4::loopback(), 8080};
    constexpr TcpEndpoint ep2{AddressV4::loopback(), 8080};
    constexpr TcpEndpoint ep3{AddressV4::loopback(), 9090};
    constexpr TcpEndpoint ep4{AddressV4::any(), 8080};

    REQUIRE(ep1 == ep2);
    REQUIRE(ep1 != ep3);
    REQUIRE(ep1 != ep4);
}

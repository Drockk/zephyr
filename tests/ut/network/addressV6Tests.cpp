#include <string>

#include <catch2/catch_test_macros.hpp>
#include <netinet/in.h>
#include <zephyr/network/addressV6.hpp>

namespace
{
using namespace zephyr::network;
}

TEST_CASE("AddressV6 - Construction", "[AddressV6][construction]")
{
    SECTION("Default constructor")
    {
        constexpr AddressV6 addr;
        REQUIRE(addr.isUnspecified());
        REQUIRE(addr.scopeId() == 0);
    }

    SECTION("From bytes array")
    {
        constexpr AddressV6::BytesType bytes{0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        constexpr AddressV6 addr{bytes};

        auto result = addr.toBytes();
        REQUIRE(result[0] == 0x20);
        REQUIRE(result[1] == 0x01);
        REQUIRE(result[15] == 1);
    }

    SECTION("From bytes with scope")
    {
        constexpr AddressV6::BytesType bytes{};
        constexpr AddressV6 addr{bytes, 42};

        REQUIRE(addr.scopeId() == 42);
    }

    SECTION("From sockaddr_in6")
    {
        sockaddr_in6 sockAddr{};
        sockAddr.sin6_family = AF_INET6;
        sockAddr.sin6_scope_id = 5;
        // Loopback ::1
        sockAddr.sin6_addr.s6_addr[15] = 1;

        AddressV6 addr{sockAddr};
        REQUIRE(addr.isLoopback());
        REQUIRE(addr.scopeId() == 5);
    }
}

TEST_CASE("AddressV6 - Special addresses", "[AddressV6][special]")
{
    SECTION("any()")
    {
        constexpr auto addr = AddressV6::any();
        REQUIRE(addr.isUnspecified());
    }

    SECTION("loopback()")
    {
        constexpr auto addr = AddressV6::loopback();
        REQUIRE(addr.isLoopback());

        auto bytes = addr.toBytes();
        for (size_t i = 0; i < 15; ++i) {
            REQUIRE(bytes[i] == 0);
        }
        REQUIRE(bytes[15] == 1);
    }
}

TEST_CASE("AddressV6 - Properties", "[AddressV6][properties]")
{
    SECTION("isLoopback()")
    {
        constexpr AddressV6::BytesType loopbackBytes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        constexpr AddressV6 loopback{loopbackBytes};
        constexpr AddressV6 notLoopback{AddressV6::BytesType{1}};

        REQUIRE(loopback.isLoopback());
        REQUIRE_FALSE(notLoopback.isLoopback());
    }

    SECTION("isMulticast()")
    {
        constexpr AddressV6::BytesType multicastBytes{0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        constexpr AddressV6 multicast{multicastBytes};
        constexpr AddressV6 notMulticast{AddressV6::BytesType{}};

        REQUIRE(multicast.isMulticast());
        REQUIRE_FALSE(notMulticast.isMulticast());
    }

    SECTION("isLinkLocal()")
    {
        constexpr AddressV6::BytesType linkLocalBytes{0xFE, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        constexpr AddressV6 linkLocal{linkLocalBytes};
        constexpr AddressV6 notLinkLocal{AddressV6::BytesType{}};

        REQUIRE(linkLocal.isLinkLocal());
        REQUIRE_FALSE(notLinkLocal.isLinkLocal());
    }

    SECTION("isUnspecified()")
    {
        constexpr AddressV6 unspec;
        constexpr AddressV6::BytesType specifiedBytes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        constexpr AddressV6 specified{specifiedBytes};

        REQUIRE(unspec.isUnspecified());
        REQUIRE_FALSE(specified.isUnspecified());
    }
}

TEST_CASE("AddressV6 - String parsing", "[AddressV6][parsing]")
{
    SECTION("Valid addresses")
    {
        auto addr1 = AddressV6::fromString("::1");
        REQUIRE(addr1.has_value());
        REQUIRE(addr1->isLoopback());

        auto addr2 = AddressV6::fromString("::");
        REQUIRE(addr2.has_value());
        REQUIRE(addr2->isUnspecified());

        auto addr3 = AddressV6::fromString("2001:db8::1");
        REQUIRE(addr3.has_value());

        auto addr4 = AddressV6::fromString("fe80::1");
        REQUIRE(addr4.has_value());
        REQUIRE(addr4->isLinkLocal());
    }

    SECTION("Addresses with scope")
    {
        auto addr1 = AddressV6::fromString("fe80::1%2");
        REQUIRE(addr1.has_value());
        REQUIRE(addr1->scopeId() == 2);

        auto addr2 = AddressV6::fromString("::1%0");
        REQUIRE(addr2.has_value());
        REQUIRE(addr2->scopeId() == 0);
    }

    SECTION("Invalid addresses")
    {
        REQUIRE_FALSE(AddressV6::fromString("").has_value());
        REQUIRE_FALSE(AddressV6::fromString("gggg::1").has_value());
        REQUIRE_FALSE(AddressV6::fromString("192.168.1.1").has_value());
        REQUIRE_FALSE(AddressV6::fromString("::::::").has_value());
    }
}

TEST_CASE("AddressV6 - String conversion", "[AddressV6][string]")
{
    SECTION("toString() basic")
    {
        auto addr1 = AddressV6::fromString("::1");
        REQUIRE(addr1->toString() == "::1");

        auto addr2 = AddressV6::fromString("::");
        REQUIRE(addr2->toString() == "::");
    }

    SECTION("toString() with scope")
    {
        auto addr = AddressV6::fromString("fe80::1%2");
        auto str = addr->toString();
        REQUIRE(str.find("%2") != std::string::npos);
    }
}

TEST_CASE("AddressV6 - Scope ID", "[AddressV6][scope]")
{
    AddressV6 addr;
    REQUIRE(addr.scopeId() == 0);

    addr.scopeId(42);
    REQUIRE(addr.scopeId() == 42);
}

TEST_CASE("AddressV6 - Comparison", "[AddressV6][comparison]")
{
    constexpr AddressV6 addr1;
    constexpr AddressV6 addr2;
    constexpr AddressV6 addr3 = AddressV6::loopback();

    REQUIRE(addr1 == addr2);
    REQUIRE(addr1 != addr3);
}

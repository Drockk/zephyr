#include <catch2/catch_test_macros.hpp>
#include <netinet/in.h>
#include <zephyr/network/addressV4.hpp>

namespace
{
using namespace zephyr::network;
}

TEST_CASE("AddressV4 - Construction", "[AddressV4][construction]")
{
    SECTION("Default constructor")
    {
        constexpr AddressV4 address;
        REQUIRE(address.toUint() == 0);
        REQUIRE(address.isUnspecified());
    }

    SECTION("From bytes array")
    {
        constexpr AddressV4::BytesType bytes{192, 168, 1, 1};
        constexpr AddressV4 address{bytes};

        auto result = address.toBytes();
        REQUIRE(result[0] == 192);
        REQUIRE(result[1] == 168);
        REQUIRE(result[2] == 1);
        REQUIRE(result[3] == 1);
    }

    SECTION("From uint32_t")
    {
        constexpr AddressV4 address{0x7F000001};  // 127.0.0.1

        REQUIRE(address.toUint() == 0x7F000001);
        auto bytes = address.toBytes();
        REQUIRE(bytes[0] == 127);
        REQUIRE(bytes[1] == 0);
        REQUIRE(bytes[2] == 0);
        REQUIRE(bytes[3] == 1);
    }

    SECTION("From sockaddr_in")
    {
        sockaddr_in sockAddress{};
        sockAddress.sin_family = AF_INET;
        sockAddress.sin_addr.s_addr = htonl(0xC0A80001);  // 192.168.0.1

        AddressV4 address{sockAddress};
        REQUIRE(address.toUint() == 0xC0A80001);
    }
}

TEST_CASE("AddressV4 - Special addresses", "[AddressV4][special]")
{
    SECTION("any()")
    {
        constexpr auto address = AddressV4::any();
        REQUIRE(address.toUint() == 0);
        REQUIRE(address.isUnspecified());
    }

    SECTION("loopback()")
    {
        constexpr auto address = AddressV4::loopback();
        REQUIRE(address.isLoopback());

        auto bytes = address.toBytes();
        REQUIRE(bytes[0] == 127);
    }

    SECTION("broadcast()")
    {
        constexpr auto address = AddressV4::broadcast();
        REQUIRE(address.toUint() == 0xFFFFFFFF);
    }
}

TEST_CASE("AddressV4 - Properties", "[AddressV4][properties]")
{
    SECTION("is_loopback()")
    {
        constexpr AddressV4 loopback{AddressV4::BytesType{127, 0, 0, 1}};
        constexpr AddressV4 loopback2{AddressV4::BytesType{127, 255, 255, 255}};
        constexpr AddressV4 notLoopback{AddressV4::BytesType{192, 168, 1, 1}};

        REQUIRE(loopback.isLoopback());
        REQUIRE(loopback2.isLoopback());
        REQUIRE_FALSE(notLoopback.isLoopback());
    }

    SECTION("is_multicast()")
    {
        constexpr AddressV4 multicast{AddressV4::BytesType{224, 0, 0, 1}};
        constexpr AddressV4 multicast2{AddressV4::BytesType{239, 255, 255, 255}};
        constexpr AddressV4 notMulticast{AddressV4::BytesType{192, 168, 1, 1}};

        REQUIRE(multicast.isMulticast());
        REQUIRE(multicast2.isMulticast());
        REQUIRE_FALSE(notMulticast.isMulticast());
    }

    SECTION("is_unspecified()")
    {
        constexpr AddressV4 unspec{0};
        constexpr AddressV4 specified{AddressV4::BytesType{1, 2, 3, 4}};

        REQUIRE(unspec.isUnspecified());
        REQUIRE_FALSE(specified.isUnspecified());
    }
}

TEST_CASE("AddressV4 - String parsing", "[AddressV4][parsing]")
{
    SECTION("Valid addresses")
    {
        auto addr1 = AddressV4::fromString("192.168.1.1");
        REQUIRE(addr1.has_value());
        REQUIRE(addr1->toUint() == 0xC0A80101);

        auto addr2 = AddressV4::fromString("127.0.0.1");
        REQUIRE(addr2.has_value());
        REQUIRE(addr2->isLoopback());

        auto addr3 = AddressV4::fromString("0.0.0.0");
        REQUIRE(addr3.has_value());
        REQUIRE(addr3->isUnspecified());

        auto addr4 = AddressV4::fromString("255.255.255.255");
        REQUIRE(addr4.has_value());
        REQUIRE(addr4->toUint() == 0xFFFFFFFF);
    }

    SECTION("Invalid addresses")
    {
        REQUIRE_FALSE(AddressV4::fromString("").has_value());
        REQUIRE_FALSE(AddressV4::fromString("256.1.1.1").has_value());
        REQUIRE_FALSE(AddressV4::fromString("1.1.1").has_value());
        REQUIRE_FALSE(AddressV4::fromString("1.1.1.1.1").has_value());
        REQUIRE_FALSE(AddressV4::fromString("abc.def.ghi.jkl").has_value());
        REQUIRE_FALSE(AddressV4::fromString("192.168.-1.1").has_value());
    }
}

TEST_CASE("AddressV4 - String conversion", "[AddressV4][string]")
{
    SECTION("to_string() roundtrip")
    {
        auto addr1 = AddressV4::fromString("192.168.1.1");
        REQUIRE(addr1->toString() == "192.168.1.1");

        auto addr2 = AddressV4::fromString("127.0.0.1");
        REQUIRE(addr2->toString() == "127.0.0.1");

        auto addr3 = AddressV4::fromString("0.0.0.0");
        REQUIRE(addr3->toString() == "0.0.0.0");
    }
}

TEST_CASE("AddressV4 - Comparison", "[AddressV4][comparison]")
{
    constexpr AddressV4 addr1{AddressV4::BytesType{192, 168, 1, 1}};
    constexpr AddressV4 addr2{AddressV4::BytesType{192, 168, 1, 1}};
    constexpr AddressV4 addr3{AddressV4::BytesType{192, 168, 1, 2}};

    REQUIRE(addr1 == addr2);
    REQUIRE(addr1 != addr3);
    REQUIRE(addr1 < addr3);
    REQUIRE(addr3 > addr1);
}

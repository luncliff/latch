//
//  32.8 Coordination types     [thread.coord]
//  32.8.1 Latches              [thread.latch]
//  32.8.1.2 class latch        [thread.latch.class]
//
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <future>
#include <latch.hpp>

using namespace std;

TEST_CASE("latch ctor zero") {
    latch wg{0}; // Expects: expected >= 0 is true
    REQUIRE(wg.try_wait()); // counter == 0
}

TEST_CASE("latch ctor negative") {
    latch wg{-1}; // Expects: expected >= 0 is true

    auto update = 0;
    // Expects: update >= 0 is true and update <= counter is true
    REQUIRE(update >= 0);
    // !!! update <= counter is false !!!
    REQUIRE_THROWS_AS(wg.count_down(update), std::system_error);
}

TEST_CASE("latch ctor positive") {
    latch wg{1}; // Expects: expected >= 0 is true

    SECTION("count_down and wait") {
        REQUIRE_NOTHROW(wg.count_down(1));
        wg.wait();
    }
    SECTION("count_down and try_wait") {
        REQUIRE_NOTHROW(wg.count_down(1));
        REQUIRE(wg.try_wait());
    }
    SECTION("arrive_and_wait") {
        REQUIRE_NOTHROW(wg.arrive_and_wait(1));
    }
}

TEST_CASE("latch wait") {
    latch wg{2};

    SECTION("wait then count_down") {
        auto f = async(launch::async, [&wg]() -> void {
            // blocks on *this until a call to count_down that decrements counter to zero
            return wg.wait();
        });
        this_thread::sleep_for(300ms);

        REQUIRE_NOTHROW(wg.count_down(1));
        REQUIRE_NOTHROW(wg.count_down(1));
        REQUIRE_NOTHROW(f.get()); // wait returned without exception
    }
    SECTION("count_down then wait") {
        REQUIRE_NOTHROW(wg.count_down(2));
        REQUIRE_NOTHROW(wg.wait()); // already reached 0
    }
    SECTION("wait mutliple times") {
        // wg.count_down(2);
        // wg.wait();
        REQUIRE_NOTHROW(wg.arrive_and_wait(2));
        REQUIRE_NOTHROW(wg.wait()); // if counter equals zero, returns immediately
    }
}

TEST_CASE("latch try_wait") {
    latch wg{1};

    SECTION("wait then count_down") {
        auto f = async(launch::async, [&wg]() -> bool {
            // With very low probability false, Otherwise counter == 0
            return wg.try_wait();
        });
        this_thread::sleep_for(300ms);

        REQUIRE_NOTHROW(wg.count_down(1));
        REQUIRE(f.get() == true); // TODO: check system error code toghether
    }
}

TEST_CASE("latch awake") {
    latch wg{3};

    SECTION("awake multiple threads") {
        auto f1 = async(launch::async, [&wg]() -> bool { return wg.try_wait(); });
        auto f2 = async(launch::async, [&wg]() -> void { return wg.wait(); });
        auto f3 = async(launch::async, [&wg]() -> void { return wg.arrive_and_wait(2); });
        this_thread::sleep_for(300ms);

        REQUIRE_NOTHROW(wg.count_down(1));
        REQUIRE(f1.get() == true);
        REQUIRE_NOTHROW(f2.get());
        REQUIRE_NOTHROW(f3.get());
    }
}

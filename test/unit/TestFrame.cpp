// NOLINTBEGIN(readability-magic-numbers, readability-function-cognitive-complexity)
#include <catch2/catch_test_macros.hpp>

#include "TestFrame.h"

TEST_CASE("Test frame")
{
    TestFrame frame;
    SECTION("test frame contains valid ethernet header")
    {
        // Check for EtherCAT ehtertype
        CHECK(frame.header.h_proto == 0xa488);

        // The ESC sets bit 1 of SOURCE_MAC[1] to be able to distinguish between frames transmitted by the master and
        // frames received by the master
        // check that this bit is not set
        CHECK((frame.header.h_source[1] & (1 << 1)) == 0);
    }

    SECTION("test frame contains NOP command")
    {
        // Check frame header
        CHECK(frame.ecatFrameHeader.length == 10 + 2);  // 10 bytes datagram header + 2 bytes wkc
        CHECK(frame.datagramHeader.command == 0);       // NOP command
    }
}

// NOLINTEND(readability-magic-numbers, readability-function-cognitive-complexity)

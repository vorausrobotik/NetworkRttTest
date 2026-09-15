#include "TestFrame.h"

#include <arpa/inet.h>
#include <linux/if_ether.h>

// NOLINTBEGIN(*-magic-numbers)

namespace vr::NetworkRttTest
{

void setValidEthernetHeader(ethhdr& header)
{
    // use a valid src mac address, use broadcast as destination mac
    header.h_source[0] = 0x1;
    header.h_source[1] = 0x1;
    header.h_source[2] = 0x1;
    header.h_source[3] = 0x1;
    header.h_source[4] = 0x1;
    header.h_source[5] = 0x1;

    header.h_dest[0] = 0xFF;
    header.h_dest[1] = 0xFF;
    header.h_dest[2] = 0xFF;
    header.h_dest[3] = 0xFF;
    header.h_dest[4] = 0xFF;
    header.h_dest[5] = 0xFF;

    header.h_proto = htons(ETHERCAT_ETHERTYPE);  // ethercat packet type
}

TestFrame::TestFrame()
{
    setValidEthernetHeader(header);

    ecatFrameHeader.length = sizeof(DatagramHeader) + 2;  // datagram header and working counter
    ecatFrameHeader.type = 1;
}

ReadSlaveTimeFrame::ReadSlaveTimeFrame()
{
    setValidEthernetHeader(header);
    ecatFrameHeader.length = sizeof(DatagramHeader) + 8 + 2;  // datagram header + timestamp + wkc
    ecatFrameHeader.type = 1;
    datagramHeader.command = 1;       // APRD
    datagramHeader.address1 = 0;      // first slave
    datagramHeader.address2 = 0x910;  // time
    datagramHeader.length = 8;        // read 8 bytes
}

}  // namespace vr::NetworkRttTest

// NOLINTEND(*-magic-numbers)

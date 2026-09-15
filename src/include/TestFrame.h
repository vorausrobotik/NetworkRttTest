#pragma once
#include <linux/if_ether.h>
#include <cstdint>

constexpr uint16_t ETHERCAT_ETHERTYPE = 0x88A4;

struct FrameHeader
{
    uint16_t length : 11;
    uint16_t : 1;
    uint8_t type : 4;
} __attribute__((packed));

struct DatagramHeader
{
    uint8_t command;
    uint8_t index;
    uint16_t address1;
    uint16_t address2;
    uint16_t length : 11;
    uint16_t : 3;  // reserved
    uint16_t circulating : 1;
    uint16_t next : 1;

    uint16_t irq;
} __attribute__((packed));

void setValidEthernetHeader(ethhdr& header);

/*
 * Testframe which just contains a NOP.
 */
struct TestFrame
{
    TestFrame();
    ethhdr header{};
    FrameHeader ecatFrameHeader{};
    DatagramHeader datagramHeader{};
    uint16_t wkc{};
} __attribute__((packed));

struct ReadSlaveTimeFrame
{
    ReadSlaveTimeFrame();

    ethhdr header{};
    FrameHeader ecatFrameHeader{};
    DatagramHeader datagramHeader{};
    uint64_t timestamp{};
    uint16_t wkc{};
} __attribute__((packed));

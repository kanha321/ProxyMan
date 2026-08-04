#ifndef PACKET_HEADERS_H
#define PACKET_HEADERS_H

#include <cstdint>
#include <winsock2.h>

#pragma pack(push, 1)
struct IPv4Header {
    uint8_t  ver_ihl;      // Version (4 bits) + Header Length (4 bits)
    uint8_t  tos;          // Type of Service
    uint16_t length;       // Total Length
    uint16_t id;           // Identification
    uint16_t frag_off;     // Flags + Fragment Offset
    uint8_t  ttl;          // Time to Live
    uint8_t  protocol;     // Protocol (6 = TCP, 17 = UDP)
    uint16_t checksum;     // Header Checksum
    uint32_t srcAddr;      // Source Address
    uint32_t dstAddr;      // Destination Address

    uint8_t version() const { return (ver_ihl >> 4) & 0x0F; }
    uint8_t headerLenBytes() const { return (ver_ihl & 0x0F) * 4; }
};

struct TcpHeader {
    uint16_t srcPort;      // Source Port
    uint16_t dstPort;      // Destination Port
    uint32_t seqNum;       // Sequence Number
    uint32_t ackNum;       // Acknowledgment Number
    uint8_t  dataOffset;   // Data Offset (4 bits) + Reserved (4 bits)
    uint8_t  flags;        // Flags (FIN, SYN, RST, PSH, ACK, URG)
    uint16_t window;       // Window Size
    uint16_t checksum;     // Checksum
    uint16_t urgPtr;       // Urgent Pointer

    bool isSyn() const { return (flags & 0x02) != 0; }
    bool isAck() const { return (flags & 0x10) != 0; }
    bool isFin() const { return (flags & 0x01) != 0; }
    bool isRst() const { return (flags & 0x04) != 0; }
    uint8_t headerLenBytes() const { return ((dataOffset >> 4) & 0x0F) * 4; }
};
#pragma pack(pop)

#endif // PACKET_HEADERS_H

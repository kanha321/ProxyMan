#include "dns_interceptor.h"
#include "packet_headers.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windivert.h>
#include <iostream>
#include <vector>
#include <cstring>

uint32_t DnsTable::GetOrAllocateSyntheticIp(const std::string& domain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_domainToIp.find(domain);
    if (it != m_domainToIp.end()) {
        return it->second;
    }

    // Allocate synthetic IP in 198.18.X.Y range (RFC 2544 Benchmark range)
    uint32_t counter = m_nextIpCounter++;
    uint8_t b3 = (counter >> 16) & 0xFF;
    uint8_t b4 = (counter >> 8) & 0xFF;
    uint8_t b5 = counter & 0xFF;

    // 198.18.b4.b5
    in_addr addr{};
    addr.S_un.S_un_b.s_b1 = 198;
    addr.S_un.S_un_b.s_b2 = 18;
    addr.S_un.S_un_b.s_b3 = b4;
    addr.S_un.S_un_b.s_b4 = (b5 == 0 || b5 == 255) ? 1 : b5;

    uint32_t ipNet = addr.s_addr;
    m_domainToIp[domain] = ipNet;
    m_ipToDomain[ipNet] = domain;

    return ipNet;
}

bool DnsTable::LookupDomain(uint32_t ipNet, std::string& domain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_ipToDomain.find(ipNet);
    if (it != m_ipToDomain.end()) {
        domain = it->second;
        return true;
    }
    return false;
}

StoppableDnsInterceptor::~StoppableDnsInterceptor() {
    Stop();
}

void StoppableDnsInterceptor::Start(DnsTable* table) {
    if (m_running.load()) return;
    m_stopping = false;
    m_workerThread = std::thread(&StoppableDnsInterceptor::InterceptorLoop, this, table);
    m_running = true;
}

void StoppableDnsInterceptor::Stop() {
    if (!m_running.load()) return;
    m_stopping = true;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    m_running = false;
    std::cout << "[DnsInterceptor] Stopped." << std::endl;
}

static std::string ParseDnsQName(const uint8_t* buf, size_t len, size_t& offset) {
    std::string qname;
    size_t pos = offset;
    while (pos < len) {
        uint8_t labelLen = buf[pos];
        if (labelLen == 0) {
            pos++;
            break;
        }
        if ((labelLen & 0xC0) == 0xC0) { // Pointer compression
            pos += 2;
            break;
        }
        if (pos + 1 + labelLen > len) break;
        if (!qname.empty()) qname += ".";
        qname.append(reinterpret_cast<const char*>(buf + pos + 1), labelLen);
        pos += 1 + labelLen;
    }
    offset = pos;
    return qname;
}

void StoppableDnsInterceptor::InterceptorLoop(DnsTable* table) {
    const char* filter = "outbound and udp and udp.DstPort == 53";
    HANDLE handle = WinDivertOpen(filter, WINDIVERT_LAYER_NETWORK, 0, 0);

    if (handle == INVALID_HANDLE_VALUE) {
        std::cerr << "[DnsInterceptor] WinDivertOpen failed. Err: " << GetLastError() << std::endl;
        return;
    }

    std::cout << "[DnsInterceptor] Started - intercepting DNS queries on UDP:53" << std::endl;

    uint8_t packet[65535];
    UINT packetLen = 0;
    WINDIVERT_ADDRESS addr;

    while (!m_stopping.load()) {
        if (!WinDivertRecv(handle, packet, sizeof(packet), &packetLen, &addr)) {
            if (m_stopping.load()) break;
            continue;
        }

        PWINDIVERT_IPHDR ipHdr = nullptr;
        PWINDIVERT_UDPHDR udpHdr = nullptr;

        WinDivertHelperParsePacket(packet, packetLen, &ipHdr, nullptr, nullptr, nullptr, nullptr, nullptr, &udpHdr, nullptr, nullptr, nullptr, nullptr);

        if (ipHdr && udpHdr) {
            uint8_t* payload = reinterpret_cast<uint8_t*>(udpHdr) + sizeof(WINDIVERT_UDPHDR);
            size_t payloadLen = packetLen - (payload - packet);

            if (payloadLen >= 12) { // Minimum DNS header length
                uint16_t dnsId = (payload[0] << 8) | payload[1];
                uint16_t dnsFlags = (payload[2] << 8) | payload[3];
                uint16_t qdCount = (payload[4] << 8) | payload[5];

                bool isQuery = ((dnsFlags & 0x8000) == 0);

                if (isQuery && qdCount >= 1) {
                    size_t offset = 12;
                    std::string domain = ParseDnsQName(payload, payloadLen, offset);

                    if (!domain.empty() && offset + 4 <= payloadLen) {
                        uint16_t qtype = (payload[offset] << 8) | payload[offset + 1];

                        if (qtype == 1) { // Type A (IPv4) query
                            uint32_t syntheticIpNet = table->GetOrAllocateSyntheticIp(domain);

                            // Build synthetic DNS Response Packet
                            uint8_t respBuf[512] = {0};
                            size_t respLen = 0;

                            // DNS Header
                            respBuf[0] = (dnsId >> 8) & 0xFF;
                            respBuf[1] = dnsId & 0xFF;
                            respBuf[2] = 0x81; // Response, Opcode=0, AA=0, TC=0, RD=1
                            respBuf[3] = 0x80; // RA=1, Z=0, RCODE=0 (No Error)
                            respBuf[4] = 0x00; respBuf[5] = 0x01; // QDCOUNT = 1
                            respBuf[6] = 0x00; respBuf[7] = 0x01; // ANCOUNT = 1
                            respBuf[8] = 0x00; respBuf[9] = 0x00; // NSCOUNT = 0
                            respBuf[10] = 0x00; respBuf[11] = 0x00; // ARCOUNT = 0
                            respLen = 12;

                            // Copy Question Section
                            size_t qLen = (offset + 4) - 12;
                            std::memcpy(respBuf + respLen, payload + 12, qLen);
                            respLen += qLen;

                            // Answer Section
                            respBuf[respLen++] = 0xC0; respBuf[respLen++] = 0x0C; // Pointer to QNAME
                            respBuf[respLen++] = 0x00; respBuf[respLen++] = 0x01; // Type A
                            respBuf[respLen++] = 0x00; respBuf[respLen++] = 0x01; // Class IN
                            respBuf[respLen++] = 0x00; respBuf[respLen++] = 0x00;
                            respBuf[respLen++] = 0x00; respBuf[respLen++] = 0x3C; // TTL = 60s
                            respBuf[respLen++] = 0x00; respBuf[respLen++] = 0x04; // RDLENGTH = 4
                            std::memcpy(respBuf + respLen, &syntheticIpNet, 4);
                            respLen += 4;

                            // Reconstruct IP and UDP headers
                            UINT32 oldSrcIp = ipHdr->SrcAddr;
                            UINT32 oldDstIp = ipHdr->DstAddr;
                            UINT16 oldSrcPort = udpHdr->SrcPort;

                            ipHdr->SrcAddr = oldDstIp;
                            ipHdr->DstAddr = oldSrcIp;
                            udpHdr->SrcPort = udpHdr->DstPort;
                            udpHdr->DstPort = oldSrcPort;

                            udpHdr->Length = htons(static_cast<uint16_t>(sizeof(WINDIVERT_UDPHDR) + respLen));
                            ipHdr->Length = htons(static_cast<uint16_t>(sizeof(WINDIVERT_IPHDR) + sizeof(WINDIVERT_UDPHDR) + respLen));

                            std::memcpy(reinterpret_cast<uint8_t*>(udpHdr) + sizeof(WINDIVERT_UDPHDR), respBuf, respLen);
                            packetLen = sizeof(WINDIVERT_IPHDR) + sizeof(WINDIVERT_UDPHDR) + static_cast<UINT>(respLen);

                            // Recalculate checksums and reinject as inbound packet to local OS
                            addr.Outbound = 0; // Mark inbound so OS DNS client receives it!
                            WinDivertHelperCalcChecksums(packet, packetLen, &addr, 0);
                            WinDivertSend(handle, packet, packetLen, NULL, &addr);
                            continue; // Intercepted & answered synthetically!
                        }
                    }
                }
            }
        }

        // Pass through unmatched DNS queries
        WinDivertSend(handle, packet, packetLen, NULL, &addr);
    }

    WinDivertClose(handle);
}

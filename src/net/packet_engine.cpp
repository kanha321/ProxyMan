#include "net/packet_engine.h"
#include "net/packet_headers.h"
#include <windivert.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <memory>
#include <iostream>
#include <thread>
#include <chrono>

// ============================================================
// Legacy blocking functions (kept for milestone test binaries)
// ============================================================

namespace {

void LegacySweepLoop(ConnTable* table) {
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        table->sweepStale(std::chrono::seconds(300));
    }
}

} // namespace

void RunPacketEngineCaptureOnly() {
    const char* filter = "tcp and outbound and !loopback";
    HANDLE handle = WinDivertOpen(filter, WINDIVERT_LAYER_NETWORK, 0, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "[PacketEngine] WinDivertOpen failed (err=%lu). Are you running as Administrator?\n", GetLastError());
        return;
    }
    std::printf("[PacketEngine] Capturing read-only outbound TCP...\n");
    const int kMaxPacket = 65535;
    auto packetBuf = std::make_unique<char[]>(kMaxPacket);
    UINT packetLen;
    WINDIVERT_ADDRESS addr;
    for (;;) {
        if (!WinDivertRecv(handle, packetBuf.get(), kMaxPacket, &packetLen, &addr)) continue;
        auto* ip = reinterpret_cast<IPv4Header*>(packetBuf.get());
        if (ip->version() == 4 && ip->protocol == 6) {
            auto* tcp = reinterpret_cast<TcpHeader*>(packetBuf.get() + ip->headerLenBytes());
            if (tcp->isSyn() && !tcp->isAck()) {
                char srcIpStr[INET_ADDRSTRLEN], dstIpStr[INET_ADDRSTRLEN];
                in_addr srcAddr; srcAddr.s_addr = ip->srcAddr;
                in_addr dstAddr; dstAddr.s_addr = ip->dstAddr;
                inet_ntop(AF_INET, &srcAddr, srcIpStr, sizeof(srcIpStr));
                inet_ntop(AF_INET, &dstAddr, dstIpStr, sizeof(dstIpStr));
                std::printf("[CAPTURE SYN] %s:%u -> %s:%u\n", srcIpStr, ntohs(tcp->srcPort), dstIpStr, ntohs(tcp->dstPort));
            }
        }
        WinDivertSend(handle, packetBuf.get(), packetLen, nullptr, &addr);
    }
}

void RunPacketEngine(ConnTable& table, const Config& cfg) {
    std::string filter = "tcp and ((outbound and !loopback) or (outbound and loopback and tcp.SrcPort == " + std::to_string(cfg.relayPort) + "))";
    HANDLE handle = WinDivertOpen(filter.c_str(), WINDIVERT_LAYER_NETWORK, 0, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "[PacketEngine] WinDivertOpen failed (err=%lu). Are you running as Administrator?\n", GetLastError());
        return;
    }
    std::thread(LegacySweepLoop, &table).detach();
    in_addr proxyInAddr{};
    inet_pton(AF_INET, cfg.proxyIp.c_str(), &proxyInAddr);
    uint32_t proxyAddrNet = proxyInAddr.s_addr;
    uint16_t proxyPortNet = htons(cfg.proxyPort);
    const int kMaxPacket = 65535;
    auto packetBuf = std::make_unique<char[]>(kMaxPacket);
    UINT packetLen;
    WINDIVERT_ADDRESS addr;
    std::printf("[PacketEngine] Intercepting & Redirecting TCP flows to 127.0.0.1:%u...\n", cfg.relayPort);
    for (;;) {
        if (!WinDivertRecv(handle, packetBuf.get(), kMaxPacket, &packetLen, &addr)) continue;
        auto* ip = reinterpret_cast<IPv4Header*>(packetBuf.get());
        if (ip->version() != 4 || ip->protocol != 6) { WinDivertSend(handle, packetBuf.get(), packetLen, nullptr, &addr); continue; }
        auto* tcp = reinterpret_cast<TcpHeader*>(packetBuf.get() + ip->headerLenBytes());
        uint16_t srcPort = ntohs(tcp->srcPort);
        uint16_t dstPort = ntohs(tcp->dstPort);
        bool isReturnLeg = (addr.Loopback && srcPort == cfg.relayPort);
        if (!isReturnLeg) {
            bool isTrafficToRealProxy = (ip->dstAddr == proxyAddrNet && tcp->dstPort == proxyPortNet);
            if (!isTrafficToRealProxy) {
                if (tcp->isSyn() && !tcp->isAck()) table.insert(srcPort, ip->dstAddr, tcp->dstPort);
                uint32_t mappedAddr; uint16_t mappedPort;
                if (table.lookup(srcPort, mappedAddr, mappedPort)) { ip->dstAddr = htonl(INADDR_LOOPBACK); tcp->dstPort = htons(cfg.relayPort); }
            }
        } else {
            uint32_t mappedAddr; uint16_t mappedPort;
            if (table.lookup(dstPort, mappedAddr, mappedPort)) { ip->srcAddr = mappedAddr; tcp->srcPort = mappedPort; }
        }
        WinDivertHelperCalcChecksums(packetBuf.get(), packetLen, &addr, 0);
        WinDivertSend(handle, packetBuf.get(), packetLen, nullptr, &addr);
    }
}

// ============================================================
// StoppablePacketEngine (Milestone 10)
// ============================================================

StoppablePacketEngine::~StoppablePacketEngine() {
    Stop();
}

void StoppablePacketEngine::Start(ConnTable& table, const Config& cfg) {
    if (m_running.load()) return;
    m_stopping = false;
    m_captureThread = std::thread(&StoppablePacketEngine::CaptureLoop, this, &table, &cfg);
    m_sweepThread = std::thread(&StoppablePacketEngine::SweepLoop, this, &table);
    m_running = true;
}

void StoppablePacketEngine::Stop() {
    if (!m_running.load()) return;
    m_stopping = true;

    // Close the WinDivert handle to unblock WinDivertRecv
    if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE) {
        WinDivertClose(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }

    // Wake sweep thread
    m_sweepCv.notify_all();

    if (m_captureThread.joinable()) m_captureThread.join();
    if (m_sweepThread.joinable()) m_sweepThread.join();
    m_running = false;
    std::printf("[PacketEngine] Stopped.\n");
}

void StoppablePacketEngine::SweepLoop(ConnTable* table) {
    while (!m_stopping.load()) {
        std::unique_lock<std::mutex> lock(m_sweepMutex);
        m_sweepCv.wait_for(lock, std::chrono::seconds(30), [this]() { return m_stopping.load(); });
        if (m_stopping.load()) break;
        table->sweepStale(std::chrono::seconds(300));
    }
}

static HANDLE OpenWinDivertWithRetry(const char* filter, WINDIVERT_LAYER layer, INT16 priority, UINT64 flags) {
    HANDLE handle = INVALID_HANDLE_VALUE;
    for (int attempt = 1; attempt <= 10; ++attempt) {
        handle = WinDivertOpen(filter, layer, priority, flags);
        if (handle != INVALID_HANDLE_VALUE) {
            return handle;
        }
        DWORD err = GetLastError();
        if (err == 1058 || err == 1056 || err == 1060 || err == 1053) {
            Sleep(200);
            continue;
        }
        break;
    }
    return handle;
}

void StoppablePacketEngine::CaptureLoop(ConnTable* table, const Config* cfg) {
    std::string filter = "tcp and ((outbound and !loopback) or (outbound and loopback and tcp.SrcPort == " + std::to_string(cfg->relayPort) + "))";
    HANDLE handle = OpenWinDivertWithRetry(filter.c_str(), WINDIVERT_LAYER_NETWORK, 0, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "[PacketEngine] WinDivertOpen failed (err=%lu). Are you running as Administrator?\n", GetLastError());
        return;
    }
    m_handle = handle;

    in_addr proxyInAddr{};
    inet_pton(AF_INET, cfg->proxyIp.c_str(), &proxyInAddr);
    uint32_t proxyAddrNet = proxyInAddr.s_addr;
    uint16_t proxyPortNet = htons(cfg->proxyPort);

    const int kMaxPacket = 65535;
    auto packetBuf = std::make_unique<char[]>(kMaxPacket);
    UINT packetLen;
    WINDIVERT_ADDRESS addr;

    std::printf("[PacketEngine] Started - intercepting TCP flows to 127.0.0.1:%u\n", cfg->relayPort);

    while (!m_stopping.load()) {
        if (!WinDivertRecv(handle, packetBuf.get(), kMaxPacket, &packetLen, &addr)) {
            if (m_stopping.load()) break;
            continue;
        }

        auto* ip = reinterpret_cast<IPv4Header*>(packetBuf.get());
        if (ip->version() != 4 || ip->protocol != 6) {
            WinDivertSend(handle, packetBuf.get(), packetLen, nullptr, &addr);
            continue;
        }

        auto* tcp = reinterpret_cast<TcpHeader*>(packetBuf.get() + ip->headerLenBytes());
        uint16_t srcPort = ntohs(tcp->srcPort);
        uint16_t dstPort = ntohs(tcp->dstPort);
        bool isReturnLeg = (addr.Loopback && srcPort == cfg->relayPort);

        if (!isReturnLeg) {
            bool isTrafficToRealProxy = (ip->dstAddr == proxyAddrNet && tcp->dstPort == proxyPortNet);
            if (!isTrafficToRealProxy) {
                if (tcp->isSyn() && !tcp->isAck()) {
                    table->insert(srcPort, ip->dstAddr, tcp->dstPort);
                }
                uint32_t mappedAddr; uint16_t mappedPort;
                if (table->lookup(srcPort, mappedAddr, mappedPort)) {
                    ip->dstAddr = htonl(INADDR_LOOPBACK);
                    tcp->dstPort = htons(cfg->relayPort);
                }
            }
        } else {
            uint32_t mappedAddr; uint16_t mappedPort;
            if (table->lookup(dstPort, mappedAddr, mappedPort)) {
                ip->srcAddr = mappedAddr;
                tcp->srcPort = mappedPort;
            }
        }

        WinDivertHelperCalcChecksums(packetBuf.get(), packetLen, &addr, 0);
        WinDivertSend(handle, packetBuf.get(), packetLen, nullptr, &addr);
    }
}

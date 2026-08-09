#include "net/quic_blocker.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <winsock2.h>
#include <windivert.h>

// Legacy blocking function (kept for milestone test binaries)
void RunQuicBlocker() {
    const char* filter = "outbound and udp and udp.DstPort == 443";
    HANDLE handle = WinDivertOpen(filter, WINDIVERT_LAYER_NETWORK, 0, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "[QuicBlocker] WinDivertOpen failed (err=%lu).\n", GetLastError());
        return;
    }
    std::printf("[QuicBlocker] Dropping outbound UDP:443\n");
    const int kMaxPacket = 65535;
    auto packetBuf = std::make_unique<char[]>(kMaxPacket);
    UINT packetLen;
    WINDIVERT_ADDRESS addr;
    for (;;) {
        if (!WinDivertRecv(handle, packetBuf.get(), kMaxPacket, &packetLen, &addr)) continue;
    }
}

// StoppableQuicBlocker (Milestone 10)

StoppableQuicBlocker::~StoppableQuicBlocker() {
    Stop();
}

void StoppableQuicBlocker::Start() {
    if (m_running.load()) return;
    m_stopping = false;
    m_thread = std::thread(&StoppableQuicBlocker::DropLoop, this);
    m_running = true;
}

void StoppableQuicBlocker::Stop() {
    if (!m_running.load()) return;
    m_stopping = true;

    if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE) {
        WinDivertClose(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }

    if (m_thread.joinable()) m_thread.join();
    m_running = false;
    std::printf("[QuicBlocker] Stopped.\n");
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

void StoppableQuicBlocker::DropLoop() {
    const char* filter = "outbound and udp and udp.DstPort == 443";
    HANDLE handle = OpenWinDivertWithRetry(filter, WINDIVERT_LAYER_NETWORK, 0, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "[QuicBlocker] WinDivertOpen failed (err=%lu). Are you running as Administrator?\n", GetLastError());
        return;
    }
    m_handle = handle;

    std::printf("[QuicBlocker] Started - dropping outbound UDP:443 (forces TCP fallback)\n");

    const int kMaxPacket = 65535;
    auto packetBuf = std::make_unique<char[]>(kMaxPacket);
    UINT packetLen;
    WINDIVERT_ADDRESS addr;

    while (!m_stopping.load()) {
        if (!WinDivertRecv(handle, packetBuf.get(), kMaxPacket, &packetLen, &addr)) {
            if (m_stopping.load()) break;
            continue;
        }
        // Drop: do NOT reinject
    }
}

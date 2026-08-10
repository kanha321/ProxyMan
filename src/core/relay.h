#pragma once

#include "config/config.h"
#include "core/conn_table.h"
#include "net/dns_interceptor.h"
#include <thread>
#include <atomic>
#include <winsock2.h>

enum class RelayMode {
    Proxy,              // Tunnel through upstream campus proxy with auth
    DirectPassthrough   // Connect directly to target host (for Wi-Fi / Hotspot seamless fallback)
};

void RunRelay(ConnTable* table, const Config& cfg, DnsTable* dnsTable = nullptr);

class StoppableRelay {
public:
    StoppableRelay() = default;
    ~StoppableRelay();

    StoppableRelay(const StoppableRelay&) = delete;
    StoppableRelay& operator=(const StoppableRelay&) = delete;

    void Start(ConnTable* table, const Config& cfg, DnsTable* dnsTable = nullptr);
    void Stop();
    bool IsRunning() const { return m_running.load(); }

    void SetMode(RelayMode mode) { m_mode.store(mode); }
    RelayMode GetMode() const { return m_mode.load(); }

    void IncrementActiveConnections() { m_activeConnections++; }
    void DecrementActiveConnections() { m_activeConnections--; }

private:
    void AcceptLoop(ConnTable* table, const Config* cfg, DnsTable* dnsTable);

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};
    std::atomic<RelayMode> m_mode{RelayMode::Proxy};
    SOCKET m_listener = INVALID_SOCKET;
    std::thread m_acceptThread;
    std::atomic<int> m_activeConnections{0};
};

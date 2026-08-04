#ifndef RELAY_H
#define RELAY_H

#include "config.h"
#include "conn_table.h"
#include "dns_interceptor.h"
#include <thread>
#include <atomic>
#include <winsock2.h>

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

    void IncrementActiveConnections() { m_activeConnections++; }
    void DecrementActiveConnections() { m_activeConnections--; }

private:
    void AcceptLoop(ConnTable* table, const Config* cfg, DnsTable* dnsTable);

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};
    SOCKET m_listener = INVALID_SOCKET;
    std::thread m_acceptThread;
    std::atomic<int> m_activeConnections{0};
};

#endif // RELAY_H

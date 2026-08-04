#ifndef DNS_INTERCEPTOR_H
#define DNS_INTERCEPTOR_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>

class DnsTable {
public:
    uint32_t GetOrAllocateSyntheticIp(const std::string& domain);
    bool LookupDomain(uint32_t ipNet, std::string& domain);

private:
    std::mutex m_mutex;
    std::unordered_map<std::string, uint32_t> m_domainToIp;
    std::unordered_map<uint32_t, std::string> m_ipToDomain;
    uint32_t m_nextIpCounter = 0x00000100; // Starts at 198.18.0.1 (net order)
};

class StoppableDnsInterceptor {
public:
    StoppableDnsInterceptor() : m_running(false), m_stopping(false) {}
    ~StoppableDnsInterceptor();

    void Start(DnsTable* table);
    void Stop();
    bool IsRunning() const { return m_running.load(); }

private:
    void InterceptorLoop(DnsTable* table);

    std::atomic<bool> m_running;
    std::atomic<bool> m_stopping;
    std::thread m_workerThread;
};

#endif // DNS_INTERCEPTOR_H

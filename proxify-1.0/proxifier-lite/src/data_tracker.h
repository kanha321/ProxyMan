#ifndef DATA_TRACKER_H
#define DATA_TRACKER_H

#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>

struct ConnectionLogEntry {
    std::string timestamp;
    std::string processName;
    uint32_t pid = 0;
    std::string networkMode; // "Ethernet" or "Wi-Fi (SSID: xxx)"
    std::string targetHost;
    uint16_t targetPort = 0;
    std::string proxyStatus; // "TUNNELED (172.31.100.27:3128)" or "DIRECT"
    uint64_t bytesTransferred = 0;
};

class DataTracker {
public:
    static DataTracker& Instance();

    void RecordConnection(const ConnectionLogEntry& entry);
    void AddBytes(const std::string& processName, uint64_t bytes, bool isTunneled);

    void PrintSummaryReport();

private:
    DataTracker();
    ~DataTracker();

    std::mutex m_mutex;
    uint64_t m_totalBytes = 0;
    uint64_t m_tunneledBytes = 0;
    uint64_t m_directBytes = 0;

    std::unordered_map<std::string, uint64_t> m_processBytes;
    std::string m_logFilePath;
};

#endif // DATA_TRACKER_H

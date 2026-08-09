#pragma once

#include <cstdint>
#include <chrono>
#include <mutex>
#include <unordered_map>

struct RedirectTarget {
    uint32_t origDstAddr;  // network byte order (raw IPv4)
    uint16_t origDstPort;  // network byte order
    std::chrono::steady_clock::time_point lastSeen;
};

class ConnTable {
public:
    void insert(uint16_t clientPort, uint32_t dstAddr, uint16_t dstPort) {
        std::lock_guard<std::mutex> lock(mutex_);
        table_[clientPort] = RedirectTarget{dstAddr, dstPort, std::chrono::steady_clock::now()};
    }

    bool lookup(uint16_t clientPort, uint32_t& dstAddr, uint16_t& dstPort) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = table_.find(clientPort);
        if (it == table_.end()) return false;
        it->second.lastSeen = std::chrono::steady_clock::now();
        dstAddr = it->second.origDstAddr;
        dstPort = it->second.origDstPort;
        return true;
    }

    void erase(uint16_t clientPort) {
        std::lock_guard<std::mutex> lock(mutex_);
        table_.erase(clientPort);
    }

    void sweepStale(std::chrono::seconds maxAge) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        for (auto it = table_.begin(); it != table_.end();) {
            if (now - it->second.lastSeen > maxAge) it = table_.erase(it);
            else ++it;
        }
    }

private:
    std::mutex mutex_;
    std::unordered_map<uint16_t, RedirectTarget> table_;
};

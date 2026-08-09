#include "logging/data_tracker.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <cstdlib>

namespace fs = std::filesystem;

static std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

static std::string FormatBytes(uint64_t bytes) {
    double kb = bytes / 1024.0;
    double mb = kb / 1024.0;
    double gb = mb / 1024.0;

    std::stringstream ss;
    if (gb >= 1.0) {
        ss << std::fixed << std::setprecision(2) << gb << " GB";
    } else if (mb >= 1.0) {
        ss << std::fixed << std::setprecision(2) << mb << " MB";
    } else if (kb >= 1.0) {
        ss << std::fixed << std::setprecision(2) << kb << " KB";
    } else {
        ss << bytes << " B";
    }
    return ss.str();
}

DataTracker::DataTracker() {
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        fs::path logDir = fs::path(userProfile) / ".config" / "proxyman" / "logs";
        try {
            fs::create_directories(logDir);
            m_logFilePath = (logDir / "proxyman.log").string();
        } catch (...) {
            m_logFilePath = "proxyman.log";
        }
    } else {
        m_logFilePath = "proxyman.log";
    }
}

DataTracker::~DataTracker() {}

DataTracker& DataTracker::Instance() {
    static DataTracker instance;
    return instance;
}

void DataTracker::RecordConnection(const ConnectionLogEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Print live console traffic log
    std::cout << "\x1b[90m[" << entry.timestamp << "]\x1b[0m "
              << "\x1b[1;36m" << entry.processName << "\x1b[0m "
              << "\x1b[90m(PID " << entry.pid << ")\x1b[0m ➔ "
              << "\x1b[1;37m" << entry.targetHost << ":" << entry.targetPort << "\x1b[0m "
              << "[" << entry.proxyStatus << "] "
              << "\x1b[33m(" << entry.networkMode << ")\x1b[0m\n";

    // Write to persistent log file
    if (!m_logFilePath.empty()) {
        std::ofstream logFile(m_logFilePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << "[" << entry.timestamp << "] "
                    << entry.processName << " (PID " << entry.pid << ") -> "
                    << entry.targetHost << ":" << entry.targetPort << " ["
                    << entry.proxyStatus << "] (" << entry.networkMode << ")\n";
        }
    }
}

void DataTracker::AddBytes(const std::string& processName, uint64_t bytes, bool isTunneled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_totalBytes += bytes;
    if (isTunneled) {
        m_tunneledBytes += bytes;
    } else {
        m_directBytes += bytes;
    }
    m_processBytes[processName] += bytes;
}

void DataTracker::PrintSummaryReport() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::cout << "\n\x1b[1;36m===================================================================\x1b[0m\n";
    std::cout << "\x1b[1;36m  ⚡ ProxyMan Data Usage & Application Traffic Summary             \x1b[0m\n";
    std::cout << "\x1b[1;36m===================================================================\x1b[0m\n";
    std::cout << "Total Session Data Transferred: \x1b[1;32m" << FormatBytes(m_totalBytes) << "\x1b[0m\n";
    std::cout << "  • Tunneled (MNNIT Proxy):      \x1b[1;33m" << FormatBytes(m_tunneledBytes) << "\x1b[0m\n";
    std::cout << "  • Direct (Intranet Bypass):    \x1b[1;34m" << FormatBytes(m_directBytes) << "\x1b[0m\n\n";

    std::cout << "\x1b[1;32mTOP BANDWIDTH CONSUMING APPLICATIONS:\x1b[0m\n";

    struct ProcStat {
        std::string name;
        uint64_t bytes;
    };
    std::vector<ProcStat> stats;
    for (const auto& kv : m_processBytes) {
        stats.push_back({kv.first, kv.second});
    }

    std::sort(stats.begin(), stats.end(), [](const ProcStat& a, const ProcStat& b) {
        return a.bytes > b.bytes;
    });

    int rank = 1;
    for (const auto& s : stats) {
        double pct = (m_totalBytes > 0) ? (static_cast<double>(s.bytes) / m_totalBytes * 100.0) : 0.0;
        std::cout << "  " << rank++ << ". \x1b[1;36m" << s.name << "\x1b[0m"
                  << " \t" << FormatBytes(s.bytes)
                  << " (" << std::fixed << std::setprecision(1) << pct << "%)\n";
        if (rank > 10) break;
    }

    std::cout << "\nLog file location: \x1b[1;33m" << m_logFilePath << "\x1b[0m\n";
    std::cout << "\x1b[1;36m===================================================================\x1b[0m\n\n";
}

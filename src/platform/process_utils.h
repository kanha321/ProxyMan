#ifndef PROCESS_UTILS_H
#define PROCESS_UTILS_H

#include <string>
#include <cstdint>

struct ProcessInfo {
    uint32_t pid = 0;
    std::string name = "Unknown";
};

ProcessInfo GetProcessInfoFromLocalPort(uint16_t localPort);

#endif // PROCESS_UTILS_H

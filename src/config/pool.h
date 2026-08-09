#pragma once

#include "config/config.h"
#include <string>

struct ProxySelectionResult {
    std::string ip;
    uint16_t port;
    long latencyMs;
    bool found;
};

ProxySelectionResult SelectOptimalProxy(const Config& cfg);

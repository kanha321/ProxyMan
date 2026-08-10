#pragma once

#include "config/config.h"
#include <string>

struct ProxySelectionResult {
    std::string ip;
    uint16_t port;
    long latencyMs;
    bool found;
};

// Select optimal proxy with cached-first lookup and parallel pool racing
ProxySelectionResult SelectOptimalProxy(const Config& cfg, bool forceRefresh = false);

// Invalidate cached proxy choice (called on network changes or probe failures)
void InvalidateCachedProxy();

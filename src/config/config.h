#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct Config {
    std::string proxyIp = "172.31.100.25";
    uint16_t proxyPort = 3128;
    std::string proxyUser = "edcguest";
    std::string proxyPass = "edcguest";
    uint16_t relayPort = 55555;
    std::vector<std::string> proxyPool;
    std::vector<std::string> bypassList;
};

std::string GetDefaultConfigPath();
bool LoadConfigFromFile(const std::string& path, Config& config);
bool SaveConfigToFile(const std::string& path, Config& config);
bool PromptAndSaveConfig(const std::string& path, Config& config);

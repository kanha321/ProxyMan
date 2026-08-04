#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <cstdint>

struct Config {
    std::string proxyIp;
    uint16_t proxyPort = 3128;
    std::string proxyUser;
    std::string proxyPass;
    uint16_t relayPort = 55555;
};

std::string GetDefaultConfigPath();
bool LoadConfigFromFile(const std::string& path, Config& config);
bool SaveConfigToFile(const std::string& path, const Config& config);
bool PromptAndSaveConfig(const std::string& path, Config& config);

#endif // CONFIG_H

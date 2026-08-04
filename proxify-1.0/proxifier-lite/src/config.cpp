#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

static std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string StripQuotes(const std::string& s) {
    std::string str = Trim(s);
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        return str.substr(1, str.size() - 2);
    }
    if (str.size() >= 2 && str.front() == '\'' && str.back() == '\'') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

static std::vector<std::string> GetDefaultProxyPool() {
    return {
        "172.31.100.25:3128",
        "172.31.100.27:3128",
        "172.31.102.29:3128",
        "172.31.103.29:3128",
        "172.31.100.14:3128"
    };
}

static std::vector<std::string> GetDefaultBypassList() {
    return {
        "172.31.*",
        "10.*",
        "127.*",
        "localhost",
        "mnnit.ac.in"
    };
}

std::string GetDefaultConfigPath() {
    const char* userProfile = std::getenv("USERPROFILE");
    if (!userProfile) userProfile = std::getenv("HOME");

    if (userProfile) {
        fs::path p(userProfile);
        p /= ".config";
        p /= "proxyman";
        p /= "config.toml";
        return p.string();
    }
    return "config.toml";
}

static std::vector<std::string> ParseTomlArray(const std::string& arrayStr) {
    std::vector<std::string> result;
    auto start = arrayStr.find('[');
    auto end = arrayStr.rfind(']');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        return result;
    }
    std::string content = arrayStr.substr(start + 1, end - start - 1);
    std::stringstream ss(content);
    std::string token;
    while (std::getline(ss, token, ',')) {
        std::string cleaned = StripQuotes(token);
        if (!cleaned.empty()) {
            result.push_back(cleaned);
        }
    }
    return result;
}

bool LoadConfigFromFile(const std::string& path, Config& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    config.proxyPool.clear();
    config.bypassList.clear();

    std::string currentSection;
    std::string line;
    std::string multiLineBuffer;
    bool inMultiLineArray = false;
    std::string multiLineKey;

    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (inMultiLineArray) {
            multiLineBuffer += " " + line;
            if (line.find(']') != std::string::npos) {
                inMultiLineArray = false;
                auto items = ParseTomlArray(multiLineBuffer);
                if (multiLineKey == "proxy_pool") config.proxyPool = items;
                else if (multiLineKey == "bypass_list") config.bypassList = items;
            }
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            currentSection = Trim(line.substr(1, line.size() - 2));
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = Trim(line.substr(0, pos));
        std::string val = Trim(line.substr(pos + 1));

        if (val.front() == '[' && val.back() != ']') {
            inMultiLineArray = true;
            multiLineKey = key;
            multiLineBuffer = val;
            continue;
        }

        if (val.front() == '[' && val.back() == ']') {
            auto items = ParseTomlArray(val);
            if (key == "proxy_pool") config.proxyPool = items;
            else if (key == "bypass_list") config.bypassList = items;
            continue;
        }

        std::string cleanVal = StripQuotes(val);

        if (currentSection == "proxy") {
            if (key == "ip") config.proxyIp = cleanVal;
            else if (key == "port") { try { config.proxyPort = static_cast<uint16_t>(std::stoi(cleanVal)); } catch (...) {} }
            else if (key == "user" || key == "username") config.proxyUser = cleanVal;
            else if (key == "pass" || key == "password") config.proxyPass = cleanVal;
        } else {
            if (key == "proxy_ip") config.proxyIp = cleanVal;
            else if (key == "proxy_port") { try { config.proxyPort = static_cast<uint16_t>(std::stoi(cleanVal)); } catch (...) {} }
            else if (key == "proxy_user") config.proxyUser = cleanVal;
            else if (key == "proxy_pass") config.proxyPass = cleanVal;
            else if (key == "relay_port") { try { config.relayPort = static_cast<uint16_t>(std::stoi(cleanVal)); } catch (...) {} }
        }
    }

    if (config.proxyPool.empty()) config.proxyPool = GetDefaultProxyPool();
    if (config.bypassList.empty()) config.bypassList = GetDefaultBypassList();

    return !config.proxyIp.empty();
}

bool SaveConfigToFile(const std::string& path, Config& config) {
    try {
        fs::path p(path);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }

        std::ofstream file(path);
        if (!file.is_open()) return false;

        file << "# ProxyMan TOML Configuration File\n\n";
        file << "relay_port = " << config.relayPort << "\n\n";
        file << "[proxy]\n";
        file << "ip = \"" << config.proxyIp << "\"\n";
        file << "port = " << config.proxyPort << "\n";
        file << "user = \"" << config.proxyUser << "\"\n";
        file << "pass = \"" << config.proxyPass << "\"\n\n";

        file << "proxy_pool = [\n";
        if (config.proxyPool.empty()) config.proxyPool = GetDefaultProxyPool();
        for (size_t i = 0; i < config.proxyPool.size(); ++i) {
            file << "    \"" << config.proxyPool[i] << "\"" << (i + 1 < config.proxyPool.size() ? "," : "") << "\n";
        }
        file << "]\n\n";

        file << "bypass_list = [\n";
        if (config.bypassList.empty()) config.bypassList = GetDefaultBypassList();
        for (size_t i = 0; i < config.bypassList.size(); ++i) {
            file << "    \"" << config.bypassList[i] << "\"" << (i + 1 < config.bypassList.size() ? "," : "") << "\n";
        }
        file << "]\n";

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Failed to save config to " << path << ": " << e.what() << std::endl;
        return false;
    }
}

bool PromptAndSaveConfig(const std::string& path, Config& config) {
    std::cout << "==========================================================\n";
    std::cout << "  ProxyMan Initial Configuration Initialization\n";
    std::cout << "==========================================================\n";
    std::cout << "[Config] No TOML configuration file found at: " << path << "\n";

    config.proxyIp = "172.31.100.25";
    config.proxyPort = 3128;
    config.proxyUser = "edcguest";
    config.proxyPass = "edcguest";
    config.relayPort = 55555;
    config.proxyPool = GetDefaultProxyPool();
    config.bypassList = GetDefaultBypassList();

    if (SaveConfigToFile(path, config)) {
        std::cout << "[Config] Applied MNNIT defaults (172.31.100.25:3128, edcguest).\n";
        std::cout << "[Config] Saved TOML configuration to: " << path << "\n";
        std::cout << "[Config] Tip: You can customize proxy credentials or pool anytime in config.toml\n\n";
        return true;
    } else {
        std::cerr << "[Config] Failed to save TOML configuration file.\n\n";
        return false;
    }
}

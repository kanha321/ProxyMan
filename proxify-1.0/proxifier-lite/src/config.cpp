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

std::string GetDefaultConfigPath() {
    const char* userProfile = std::getenv("USERPROFILE");
    if (!userProfile) userProfile = std::getenv("HOME");

    if (userProfile) {
        fs::path p(userProfile);
        p /= ".config";
        p /= "proxyman";
        p /= "config.txt";
        return p.string();
    }
    return "proxy-config.txt";
}

bool LoadConfigFromFile(const std::string& path, Config& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = Trim(line.substr(0, pos));
        std::string val = Trim(line.substr(pos + 1));

        if (key == "proxy_ip") {
            config.proxyIp = val;
        } else if (key == "proxy_port") {
            try { config.proxyPort = static_cast<uint16_t>(std::stoi(val)); } catch (...) {}
        } else if (key == "proxy_user") {
            config.proxyUser = val;
        } else if (key == "proxy_pass") {
            config.proxyPass = val;
        } else if (key == "relay_port") {
            try { config.relayPort = static_cast<uint16_t>(std::stoi(val)); } catch (...) {}
        }
    }

    return !config.proxyIp.empty();
}

bool SaveConfigToFile(const std::string& path, const Config& config) {
    try {
        fs::path p(path);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }

        std::ofstream file(path);
        if (!file.is_open()) return false;

        file << "# ProxyMan Configuration File\n";
        file << "proxy_ip=" << config.proxyIp << "\n";
        file << "proxy_port=" << config.proxyPort << "\n";
        file << "proxy_user=" << config.proxyUser << "\n";
        file << "proxy_pass=" << config.proxyPass << "\n";
        file << "relay_port=" << config.relayPort << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Failed to save config to " << path << ": " << e.what() << std::endl;
        return false;
    }
}

bool PromptAndSaveConfig(const std::string& path, Config& config) {
    std::cout << "==========================================================\n";
    std::cout << "  ProxyMan First-Time Configuration Setup\n";
    std::cout << "==========================================================\n";
    std::cout << "No configuration found at: " << path << "\n\n";

    std::string input;

    // Proxy IP
    std::cout << "Enter Proxy IP [default: 172.31.100.25]: ";
    std::getline(std::cin, input);
    input = Trim(input);
    config.proxyIp = input.empty() ? "172.31.100.25" : input;

    // Proxy Port
    std::cout << "Enter Proxy Port [default: 3128]: ";
    std::getline(std::cin, input);
    input = Trim(input);
    if (input.empty()) {
        config.proxyPort = 3128;
    } else {
        try { config.proxyPort = static_cast<uint16_t>(std::stoi(input)); } catch (...) { config.proxyPort = 3128; }
    }

    // Username
    std::cout << "Enter Username [default: edcguest]: ";
    std::getline(std::cin, input);
    input = Trim(input);
    config.proxyUser = input.empty() ? "edcguest" : input;

    // Password
    std::cout << "Enter Password [default: edcguest]: ";
    std::getline(std::cin, input);
    input = Trim(input);
    config.proxyPass = input.empty() ? "edcguest" : input;

    // Relay Port
    std::cout << "Enter Local Relay Port [default: 55555]: ";
    std::getline(std::cin, input);
    input = Trim(input);
    if (input.empty()) {
        config.relayPort = 55555;
    } else {
        try { config.relayPort = static_cast<uint16_t>(std::stoi(input)); } catch (...) { config.relayPort = 55555; }
    }

    std::cout << "\n[Config] Saving configuration to: " << path << std::endl;
    if (SaveConfigToFile(path, config)) {
        std::cout << "[Config] Configuration saved successfully!\n\n";
        return true;
    } else {
        std::cerr << "[Config] Failed to save configuration file.\n\n";
        return false;
    }
}

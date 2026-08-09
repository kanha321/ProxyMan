#include "cli/logs.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <windows.h>

namespace fs = std::filesystem;

void ShowLogs(bool follow) {
    const char* userProfile = std::getenv("USERPROFILE");
    fs::path logPath;
    if (userProfile) {
        logPath = fs::path(userProfile) / ".config" / "proxyman" / "logs" / "proxyman.log";
    } else {
        logPath = "proxyman.log";
    }

    if (!fs::exists(logPath)) {
        std::cout << "\x1b[33m[Logs] No connection log file found at: " << logPath.string() << "\x1b[0m\n";
        return;
    }

    std::cout << "\n\x1b[1;36m===================================================================\x1b[0m\n";
    std::cout << "\x1b[1;36m  ⚡ ProxyMan Connection Log History (" << logPath.string() << ")\x1b[0m\n";
    std::cout << "\x1b[1;36m===================================================================\x1b[0m\n";

    std::ifstream file(logPath);
    if (!file.is_open()) {
        std::cout << "[Logs] Failed to open log file.\n";
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    file.close();

    size_t startIdx = (lines.size() > 25) ? (lines.size() - 25) : 0;
    for (size_t i = startIdx; i < lines.size(); ++i) {
        std::cout << "\x1b[37m" << lines[i] << "\x1b[0m\n";
    }

    if (follow) {
        std::cout << "\n\x1b[1;33m[Logs] Following live connection log stream... Press Ctrl+C to exit.\x1b[0m\n\n";
        std::ifstream tailFile(logPath);
        tailFile.seekg(0, std::ios::end);
        while (true) {
            std::string nline;
            if (std::getline(tailFile, nline)) {
                if (!nline.empty()) {
                    std::cout << "\x1b[37m" << nline << "\x1b[0m\n";
                }
            } else {
                tailFile.clear();
                Sleep(200);
            }
        }
    } else {
        std::cout << "\x1b[1;36m===================================================================\x1b[0m\n";
        std::cout << "Tip: Use \x1b[1;33mProxyMan --logs -f\x1b[0m to stream live connection logs.\n\n";
    }
}

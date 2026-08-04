#include "resources.h"

#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

#pragma comment(lib, "shell32.lib")

static void EnableANSI() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

static bool IsElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated != FALSE;
}

static bool RelaunchElevated(int argc, char* argv[]) {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return false;

    std::wstring args;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) args += L" ";
        std::string a = argv[i];
        args += std::wstring(a.begin(), a.end());
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.lpParameters = args.c_str();
    sei.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&sei) != FALSE;
}

static std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool AddToPath(const fs::path& installDir) {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_READ | KEY_WRITE, &hKey);
    bool isSystem = (lRes == ERROR_SUCCESS);
    if (!isSystem) {
        lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ | KEY_WRITE, &hKey);
    }
    if (lRes != ERROR_SUCCESS) return false;

    wchar_t currentPath[4096] = {0};
    DWORD dwSize = sizeof(currentPath);
    DWORD dwType = REG_EXPAND_SZ;

    RegQueryValueExW(hKey, L"Path", NULL, &dwType, reinterpret_cast<BYTE*>(currentPath), &dwSize);

    std::wstring pathStr(currentPath);
    std::wstring dirStr = installDir.wstring();

    if (pathStr.find(dirStr) == std::wstring::npos) {
        if (!pathStr.empty() && pathStr.back() != L';') {
            pathStr += L";";
        }
        pathStr += dirStr;

        RegSetValueExW(hKey, L"Path", 0, dwType, reinterpret_cast<const BYTE*>(pathStr.c_str()), static_cast<DWORD>((pathStr.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);

        DWORD_PTR dwResult = 0;
        SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 1000, &dwResult);
        return true;
    }
    RegCloseKey(hKey);
    return true;
}

static bool ExtractEmbeddedFile(int resourceId, const fs::path& outputPath) {
    HMODULE hModule = GetModuleHandle(NULL);
    HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(resourceId), (LPCSTR)RT_RCDATA);
    if (!hRes) {
        std::cerr << "  \x1b[31mError: Resource " << resourceId << " not found in installer binary.\x1b[0m\n";
        return false;
    }

    HGLOBAL hMem = LoadResource(hModule, hRes);
    if (!hMem) return false;

    DWORD size = SizeofResource(hModule, hRes);
    void* pData = LockResource(hMem);
    if (!pData || size == 0) return false;

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "  \x1b[31mError: Failed to open output file for writing: " << outputPath.string() << "\x1b[0m\n";
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(pData), size);
    outFile.close();
    return true;
}

static void DrawHeader() {
    std::cout << "\x1b[1;36m";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│  ⚡  ProxyMan Self-Contained Setup & Installation Wizard    │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
    std::cout << "\x1b[0m";
    std::cout << "\x1b[1;33m  💡 Tip:\x1b[0m Press \x1b[1;36m[ENTER]\x1b[0m for instant zero-configuration setup.\n\n";
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    EnableANSI();

    if (!IsElevated()) {
        std::cout << "\x1b[1;33m[Installer] Administrator privileges required for installation.\x1b[0m\n";
        std::cout << "[Installer] Requesting UAC elevation...\n";
        if (RelaunchElevated(argc, argv)) {
            return 0;
        } else {
            std::cerr << "\x1b[1;31m[Installer] Elevation request cancelled. Exiting.\x1b[0m\n";
            return 1;
        }
    }

    DrawHeader();

    // 1. Determine Default Target Install Directory
    const char* programFiles = std::getenv("ProgramFiles");
    fs::path defaultInstallDir = programFiles ? fs::path(programFiles) / "ProxyMan" : fs::path("C:\\Program Files\\ProxyMan");

    std::cout << "\x1b[1;32m[Step 1/3] Installation Directory\x1b[0m\n";
    std::cout << "Enter target folder [\x1b[1;33mdefault: " << defaultInstallDir.string() << "\x1b[0m]: ";
    std::string inputDir;
    std::getline(std::cin, inputDir);
    inputDir = Trim(inputDir);

    fs::path installDir = inputDir.empty() ? defaultInstallDir : fs::path(inputDir);

    try {
        fs::create_directories(installDir);
        std::cout << "\x1b[32m✔ Installation directory ready: " << installDir.string() << "\x1b[0m\n\n";
    } catch (const std::exception& e) {
        std::cerr << "\x1b[1;31m❌ Failed to create directory: " << e.what() << "\x1b[0m\n";
        return 1;
    }

    // 2. Write Pre-Configured MNNIT Settings (config.toml) & Display Configuration
    std::cout << "\x1b[1;32m[Step 2/3] Configuration Settings\x1b[0m\n";
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        fs::path configDir = fs::path(userProfile) / ".config" / "proxyman";
        fs::create_directories(configDir);
        fs::path configFile = configDir / "config.toml";

        std::string configContent =
            "# ProxyMan TOML Configuration File\n\n"
            "relay_port = 55555\n\n"
            "[auth]\n"
            "user = \"edcguest\"\n"
            "pass = \"edcguest\"\n\n"
            "proxy_pool = [\n"
            "    \"172.31.100.25:3128\",\n"
            "    \"172.31.100.27:3128\",\n"
            "    \"172.31.102.29:3128\",\n"
            "    \"172.31.103.29:3128\",\n"
            "    \"172.31.100.14:3128\"\n"
            "]\n\n"
            "bypass_list = [\n"
            "    \"172.31.*\",\n"
            "    \"10.*\",\n"
            "    \"127.*\",\n"
            "    \"localhost\",\n"
            "    \"mnnit.ac.in\"\n"
            "]\n";

        std::ofstream cfgFile(configFile);
        if (cfgFile.is_open()) {
            cfgFile << configContent;
            cfgFile.close();
        }

        std::cout << "\x1b[32m✔ Configuration file created at:\x1b[0m\n";
        std::cout << "  \x1b[1;33m" << configFile.string() << "\x1b[0m\n\n";

        std::cout << "\x1b[1;36m┌─ [ " << configFile.string() << " ] ────────┐\x1b[0m\n";
        std::stringstream ss(configContent);
        std::string line;
        while (std::getline(ss, line)) {
            std::cout << "\x1b[36m│\x1b[0m " << line << "\n";
        }
        std::cout << "\x1b[1;36m└──────────────────────────────────────────────────────────────┘\x1b[0m\n\n";

        std::cout << "\x1b[1;33m💡 Tip:\x1b[0m You can open and edit this \x1b[1;36mconfig.toml\x1b[0m file anytime in Notepad or VS Code\n";
        std::cout << "        to update your EDC credentials or add custom proxy IPs.\n\n";
    }

    // Extract Embedded Binaries from Installer EXE into Target Install Directory
    std::cout << "\x1b[1;32m[Extracting Embedded Binaries]\x1b[0m\n";
    struct EmbeddedFile {
        int id;
        std::string filename;
    };
    const std::vector<EmbeddedFile> embeddedFiles = {
        { IDR_PROXYMAN_EXE,  "ProxyMan.exe" },
        { IDR_WINDIVERT_DLL, "WinDivert.dll" },
        { IDR_WINDIVERT_SYS, "WinDivert64.sys" }
    };

    for (const auto& ef : embeddedFiles) {
        fs::path dst = installDir / ef.filename;
        if (ExtractEmbeddedFile(ef.id, dst)) {
            std::cout << "  Extracted: " << ef.filename << "\n";
        } else {
            std::cout << "  \x1b[33mWarning: Could not extract embedded " << ef.filename << "\x1b[0m\n";
        }
    }
    std::cout << "\n";

    // 3. System PATH Integration & Select Startup Method
    std::cout << "\x1b[1;32m[Step 3/3] System PATH & Autostart Registration\x1b[0m\n";
    if (AddToPath(installDir)) {
        std::cout << "\x1b[32m✔ Added ProxyMan directory to System PATH.\x1b[0m\n\n";
    }

    std::cout << "Choose Autostart Option:\n";
    std::cout << "  \x1b[1;36m1)\x1b[0m Task Scheduler at Logon [\x1b[1;33mRECOMMENDED — Zero UAC Prompts, Starts at Logon\x1b[0m]\n";
    std::cout << "  \x1b[1;36m2)\x1b[0m Windows System Service [\x1b[1;33mZero UAC Prompts, Starts at PC Boot\x1b[0m]\n";
    std::cout << "  \x1b[1;36m3)\x1b[0m Skip Autostart (Manual Execution Only)\n";
    std::cout << "Select choice [\x1b[1;33mdefault: 1\x1b[0m]: ";

    std::string choiceStr;
    std::getline(std::cin, choiceStr);
    choiceStr = Trim(choiceStr);
    int choice = choiceStr.empty() ? 1 : std::atoi(choiceStr.c_str());

    fs::path installedExe = installDir / "ProxyMan.exe";

    if (choice == 1) {
        std::wstring cmd = L"schtasks /create /tn \"ProxyMan\" /tr \"\\\"";
        cmd += installedExe.wstring();
        cmd += L"\\\"\" /sc ONLOGON /rl HIGHEST /f";
        int ret = _wsystem(cmd.c_str());
        if (ret == 0) {
            std::cout << "\x1b[32m✔ Task Scheduler Autostart registered successfully!\x1b[0m\n";
        } else {
            std::cout << "\x1b[31m❌ Task Scheduler registration failed.\x1b[0m\n";
        }
    } else if (choice == 2) {
        std::wstring cmd = L"sc.exe create ProxyMan binPath= \"\\\"";
        cmd += installedExe.wstring();
        cmd += L"\\\"\" start= auto DisplayName= \"ProxyMan Transparent Proxy Engine\"";
        int ret = _wsystem(cmd.c_str());
        if (ret == 0) {
            _wsystem(L"sc.exe description ProxyMan \"ProxyMan Network-Aware Transparent Proxy Engine\"");
            std::cout << "\x1b[32m✔ Windows System Service registered successfully!\x1b[0m\n";
        } else {
            std::cout << "\x1b[31m❌ Service registration failed.\x1b[0m\n";
        }
    } else {
        std::cout << "Skipped autostart registration.\n";
    }

    std::cout << "\n\x1b[1;32m";
    std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "│  🎉 ProxyMan Installation Completed Successfully!           │\n";
    std::cout << "└─────────────────────────────────────────────────────────────┘\n";
    std::cout << "\x1b[0m\n";
    std::cout << "Installed at: \x1b[1;36m" << installedExe.string() << "\x1b[0m\n";
    std::cout << "\x1b[1;33m💡 Tip:\x1b[0m Restarting your PC ensures all system applications automatically\n";
    std::cout << "        route through ProxyMan on boot.\n\n";

    std::cout << "What would you like to do next?\n";
    std::cout << "  \x1b[1;36m1)\x1b[0m Launch ProxyMan now [\x1b[1;33mRuns in background detached from console\x1b[0m]\n";
    std::cout << "  \x1b[1;36m2)\x1b[0m Restart PC now\n";
    std::cout << "  \x1b[1;36m3)\x1b[0m Exit setup (ProxyMan will start on next boot)\n";
    std::cout << "Select choice [\x1b[1;33mdefault: 1\x1b[0m]: ";

    std::string postChoiceStr;
    std::getline(std::cin, postChoiceStr);
    postChoiceStr = Trim(postChoiceStr);
    int postChoice = postChoiceStr.empty() ? 1 : std::atoi(postChoiceStr.c_str());

    if (postChoice == 1) {
        std::cout << "\n[Installer] Launching ProxyMan in background...\n";
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        std::wstring exeW = installedExe.wstring();
        if (CreateProcessW(exeW.c_str(), NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            std::cout << "\x1b[32m✔ ProxyMan engine started successfully in background!\x1b[0m\n\n";
            std::cout << "\x1b[1;33m💡 Note:\x1b[0m If any open application (browser, terminal, or IDE) stops responding,\n";
            std::cout << "        simply restart that app to apply ProxyMan proxying.\n\n";
        } else {
            std::cout << "\x1b[31m❌ Failed to start ProxyMan background process.\x1b[0m\n\n";
        }
        std::cout << "Press Enter to exit...";
        std::string dummy;
        std::getline(std::cin, dummy);
    } else if (postChoice == 2) {
        std::cout << "\n[Installer] Initiating PC restart in 5 seconds...\n";
        _wsystem(L"shutdown /r /t 5 /c \"ProxyMan Setup completed. Restarting system...\"");
        Sleep(2000);
    } else {
        std::cout << "\nExit setup. Press Enter to exit...";
        std::string dummy;
        std::getline(std::cin, dummy);
    }

    return 0;
}

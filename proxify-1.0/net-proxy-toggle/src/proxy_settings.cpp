#include "proxy_settings.h"

#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "wininet.lib")

static const wchar_t* kInternetSettingsSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";

static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string str(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], sizeNeeded, NULL, NULL);
    return str;
}

static std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], sizeNeeded);
    return wstr;
}

bool SetSystemProxy(const std::string& proxyHostPort) {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, kInternetSettingsSubKey, 0, KEY_SET_VALUE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        std::cerr << "[ProxySettings] Failed to open registry key for proxy settings. Error: " << lRes << std::endl;
        return false;
    }

    DWORD dwEnable = 1;
    RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwEnable), sizeof(dwEnable));

    std::wstring wProxyHostPort = StringToWString(proxyHostPort);
    RegSetValueExW(hKey, L"ProxyServer", 0, REG_SZ, reinterpret_cast<const BYTE*>(wProxyHostPort.c_str()), static_cast<DWORD>((wProxyHostPort.size() + 1) * sizeof(wchar_t)));

    const wchar_t* wOverride = L"<local>";
    RegSetValueExW(hKey, L"ProxyOverride", 0, REG_SZ, reinterpret_cast<const BYTE*>(wOverride), static_cast<DWORD>((wcslen(wOverride) + 1) * sizeof(wchar_t)));

    RegCloseKey(hKey);

    // Refresh WinINet settings globally
    InternetSetOptionW(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOptionW(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    std::cout << "[ProxySettings] System proxy ENABLED -> " << proxyHostPort << std::endl;
    return true;
}

bool ClearSystemProxy() {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, kInternetSettingsSubKey, 0, KEY_SET_VALUE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        std::cerr << "[ProxySettings] Failed to open registry key for proxy settings. Error: " << lRes << std::endl;
        return false;
    }

    DWORD dwEnable = 0;
    RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwEnable), sizeof(dwEnable));

    RegCloseKey(hKey);

    // Refresh WinINet settings globally
    InternetSetOptionW(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOptionW(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    std::cout << "[ProxySettings] System proxy DISABLED." << std::endl;
    return true;
}

bool GetSystemProxy(bool& enabled, std::string& proxyHostPort) {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, kInternetSettingsSubKey, 0, KEY_QUERY_VALUE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        return false;
    }

    DWORD dwEnable = 0;
    DWORD dwSize = sizeof(dwEnable);
    if (RegQueryValueExW(hKey, L"ProxyEnable", NULL, NULL, reinterpret_cast<BYTE*>(&dwEnable), &dwSize) == ERROR_SUCCESS) {
        enabled = (dwEnable != 0);
    } else {
        enabled = false;
    }

    wchar_t buf[512] = {0};
    dwSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"ProxyServer", NULL, NULL, reinterpret_cast<BYTE*>(buf), &dwSize) == ERROR_SUCCESS) {
        proxyHostPort = WStringToString(std::wstring(buf));
    } else {
        proxyHostPort.clear();
    }

    RegCloseKey(hKey);
    return true;
}

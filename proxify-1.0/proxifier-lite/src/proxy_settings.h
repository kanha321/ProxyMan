#ifndef PROXY_SETTINGS_H
#define PROXY_SETTINGS_H

#include <string>

bool SetSystemProxy(const std::string& proxyHostPort);
bool ClearSystemProxy();
bool GetSystemProxy(bool& enabled, std::string& proxyHostPort);
void ShowNotificationToast(const std::string& title, const std::string& message);

#endif // PROXY_SETTINGS_H

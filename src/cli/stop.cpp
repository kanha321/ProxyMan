#include "cli/stop.h"
#include "platform/ipc.h"
#include "platform/proxy_settings.h"
#include <windows.h>
#include <iostream>

int HandleStop() {
    bool signaled = false;
    HANDLE hStopEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\ProxyManShutdownEvent");
    if (hStopEvent != NULL) {
        SetEvent(hStopEvent);
        CloseHandle(hStopEvent);
        signaled = true;
    }

    bool killed = KillRunningProxyManProcesses();
    ClearSystemProxy();
    ShowNotificationToast("🛑 ProxyMan", "ProxyMan background engine stopped cleanly.");

    if (signaled || killed) {
        std::cout << "\x1b[32m✔ Stop signal sent to active ProxyMan background instance.\x1b[0m\n";
        std::cout << "\x1b[32m✔ System proxy settings restored.\x1b[0m\n";
    } else {
        std::cout << "\x1b[33m[Main] No running ProxyMan instance found. Cleared system proxy.\x1b[0m\n";
    }
    return 0;
}

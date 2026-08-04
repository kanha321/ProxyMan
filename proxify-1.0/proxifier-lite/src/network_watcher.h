#ifndef NETWORK_WATCHER_H
#define NETWORK_WATCHER_H

#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>

enum class LinkType {
    Ethernet,
    WiFi,
    Other,
    Unknown
};

LinkType GetActiveLinkType();
std::string LinkTypeToString(LinkType type);

class NetworkWatcher {
public:
    using Callback = std::function<void(LinkType)>;

    explicit NetworkWatcher(Callback onChange);
    ~NetworkWatcher();

    NetworkWatcher(const NetworkWatcher&) = delete;
    NetworkWatcher& operator=(const NetworkWatcher&) = delete;

private:
    static void NETIOAPI_API_ InterfaceChangeCallback(
        PVOID CallerContext,
        PMIB_IPINTERFACE_ROW Row,
        MIB_NOTIFICATION_TYPE NotificationType
    );

    void OnRawEvent();
    void DebounceWorker();

    Callback m_onChange;
    HANDLE m_notificationHandle = NULL;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_pendingEvent = false;
    uint64_t m_lastEventTimeMs = 0;
    std::atomic<bool> m_stopping{false};
    std::thread m_debounceThread;
    LinkType m_lastReportedType = LinkType::Unknown;
};

#endif // NETWORK_WATCHER_H

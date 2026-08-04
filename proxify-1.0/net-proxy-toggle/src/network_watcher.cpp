#include "network_watcher.h"

#include <vector>
#include <chrono>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

LinkType GetActiveLinkType() {
    // 1. Get default route interface index using GetBestInterface pointing to dummy IP (8.8.8.8)
    DWORD bestIfIndex = 0;
    IPAddr dummyDest = inet_addr("8.8.8.8");
    if (GetBestInterface(dummyDest, &bestIfIndex) != NO_ERROR) {
        return LinkType::Unknown;
    }

    // 2. Query adapters addresses using GetAdaptersAddresses
    ULONG family = AF_UNSPEC;
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    ULONG outBufLen = 15360; // Initial 15KB buffer
    std::vector<BYTE> buffer(outBufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

    DWORD dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
    }

    if (dwRetVal != NO_ERROR) {
        return LinkType::Unknown;
    }

    // 3. Find adapter matching bestIfIndex
    for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses != NULL; pCurrAddresses = pCurrAddresses->Next) {
        if (pCurrAddresses->IfIndex == bestIfIndex || pCurrAddresses->Ipv6IfIndex == bestIfIndex) {
            if (pCurrAddresses->IfType == IF_TYPE_ETHERNET_CSMACD) {
                return LinkType::Ethernet;
            } else if (pCurrAddresses->IfType == IF_TYPE_IEEE80211) {
                return LinkType::WiFi;
            } else {
                return LinkType::Other;
            }
        }
    }

    return LinkType::Unknown;
}

std::string LinkTypeToString(LinkType type) {
    switch (type) {
        case LinkType::Ethernet:
            return "Ethernet";
        case LinkType::WiFi:
            return "Wi-Fi";
        case LinkType::Other:
            return "Other";
        case LinkType::Unknown:
        default:
            return "Unknown";
    }
}

// NetworkWatcher Implementation

NetworkWatcher::NetworkWatcher(Callback onChange)
    : m_onChange(std::move(onChange)) {
    m_lastReportedType = GetActiveLinkType();

    // Start debounce thread
    m_debounceThread = std::thread(&NetworkWatcher::DebounceWorker, this);

    // Register interface change notification
    DWORD dwStatus = NotifyIpInterfaceChange(
        AF_UNSPEC,
        &NetworkWatcher::InterfaceChangeCallback,
        this,
        FALSE,
        &m_notificationHandle
    );

    if (dwStatus != NO_ERROR) {
        m_notificationHandle = NULL;
    }
}

NetworkWatcher::~NetworkWatcher() {
    m_stopping = true;

    if (m_notificationHandle != NULL) {
        CancelMibChangeNotify2(m_notificationHandle);
        m_notificationHandle = NULL;
    }

    m_cv.notify_all();

    if (m_debounceThread.joinable()) {
        m_debounceThread.join();
    }
}

void NETIOAPI_API_ NetworkWatcher::InterfaceChangeCallback(
    PVOID CallerContext,
    PMIB_IPINTERFACE_ROW Row,
    MIB_NOTIFICATION_TYPE NotificationType) {
    (void)Row;
    (void)NotificationType;

    if (CallerContext != nullptr) {
        auto* watcher = static_cast<NetworkWatcher*>(CallerContext);
        watcher->OnRawEvent();
    }
}

void NetworkWatcher::OnRawEvent() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastEventTimeMs = GetTickCount64();
    m_pendingEvent = true;
    m_cv.notify_one();
}

void NetworkWatcher::DebounceWorker() {
    const uint64_t kDebounceDelayMs = 1500;

    while (!m_stopping) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this]() {
            return m_pendingEvent || m_stopping;
        });

        if (m_stopping) {
            break;
        }

        while (m_pendingEvent && !m_stopping) {
            uint64_t now = GetTickCount64();
            uint64_t elapsed = (now >= m_lastEventTimeMs) ? (now - m_lastEventTimeMs) : kDebounceDelayMs;

            if (elapsed < kDebounceDelayMs) {
                uint64_t waitTime = kDebounceDelayMs - elapsed;
                m_cv.wait_for(lock, std::chrono::milliseconds(waitTime));
            } else {
                m_pendingEvent = false;
                lock.unlock();

                LinkType currentType = GetActiveLinkType();
                if (currentType != m_lastReportedType) {
                    m_lastReportedType = currentType;
                    if (m_onChange) {
                        m_onChange(currentType);
                    }
                }

                lock.lock();
            }
        }
    }
}

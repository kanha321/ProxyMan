#include "wifi_utils.h"

#include <windows.h>
#include <wlanapi.h>
#include <iostream>

#pragma comment(lib, "wlanapi.lib")

std::string GetActiveWifiSSID() {
    HANDLE hClient = NULL;
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;
    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS || !hClient) {
        return "";
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
    if (dwResult != ERROR_SUCCESS || !pIfList) {
        WlanCloseHandle(hClient, NULL);
        return "";
    }

    std::string ssidName = "";

    for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
        PWLAN_INTERFACE_INFO pIfInfo = (WLAN_INTERFACE_INFO*)&pIfList->InterfaceInfo[i];
        if (pIfInfo->isState == wlan_interface_state_connected) {
            PWLAN_CONNECTION_ATTRIBUTES pConnAttr = NULL;
            DWORD dataSize = 0;
            WLAN_OPCODE_VALUE_TYPE opCode = wlan_opcode_value_type_invalid;

            dwResult = WlanQueryInterface(
                hClient,
                &pIfInfo->InterfaceGuid,
                wlan_intf_opcode_current_connection,
                NULL,
                &dataSize,
                (PVOID*)&pConnAttr,
                &opCode
            );

            if (dwResult == ERROR_SUCCESS && pConnAttr) {
                ULONG len = pConnAttr->wlanAssociationAttributes.dot11Ssid.uSSIDLength;
                if (len > 0 && len <= 32) {
                    ssidName = std::string((char*)pConnAttr->wlanAssociationAttributes.dot11Ssid.ucSSID, len);
                }
                WlanFreeMemory(pConnAttr);
            }
            if (!ssidName.empty()) break;
        }
    }

    if (pIfList) WlanFreeMemory(pIfList);
    WlanCloseHandle(hClient, NULL);

    return ssidName;
}

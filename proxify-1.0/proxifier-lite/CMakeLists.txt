cmake_minimum_required(VERSION 3.15)
project(ProxyMan CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O1 /GL /Gy /Gw /EHsc)
    add_link_options(/LTCG /OPT:REF /OPT:ICF)
endif()

set(WINDIVERT_DIR "${CMAKE_SOURCE_DIR}/../WinDivert-2.2.2-A")

include_directories(
    src
    "${WINDIVERT_DIR}/include"
)

# ============================================================
# Production Binary: ProxyMan.exe
# ============================================================
add_executable(ProxyMan WIN32
    src/main.cpp
    src/engine_controller.cpp
    src/engine_controller.h
    src/network_watcher.cpp
    src/network_watcher.h
    src/proxy_settings.cpp
    src/proxy_settings.h
    src/dns_interceptor.cpp
    src/dns_interceptor.h
    src/packet_engine.cpp
    src/packet_engine.h
    src/packet_headers.h
    src/quic_blocker.cpp
    src/quic_blocker.h
    src/relay.cpp
    src/relay.h
    src/conn_table.h
    src/config.cpp
    src/config.h
    src/http_proxy_client.cpp
    src/http_proxy_client.h
    src/process_utils.cpp
    src/process_utils.h
    src/wifi_utils.cpp
    src/wifi_utils.h
    src/data_tracker.cpp
    src/data_tracker.h
)
set_target_properties(ProxyMan PROPERTIES OUTPUT_NAME "ProxyMan")
target_link_libraries(ProxyMan PRIVATE ws2_32 iphlpapi wininet wlanapi psapi "${WINDIVERT_DIR}/x64/WinDivert.lib")

# Copy WinDivert driver files for ProxyMan execution
add_custom_command(TARGET ProxyMan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WINDIVERT_DIR}/x64/WinDivert.dll"
        "$<TARGET_FILE_DIR:ProxyMan>/WinDivert.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WINDIVERT_DIR}/x64/WinDivert64.sys"
        "$<TARGET_FILE_DIR:ProxyMan>/WinDivert64.sys"
)

# ============================================================
# Standalone Self-Contained Installer: ProxyManSetup.exe
# Embeds ProxyMan.exe, WinDivert.dll, and WinDivert64.sys
# ============================================================
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/Release/ProxyMan.exe" PROXYMAN_EXE_PATH)
file(TO_CMAKE_PATH "${WINDIVERT_DIR}/x64/WinDivert.dll" WINDIVERT_DLL_PATH)
file(TO_CMAKE_PATH "${WINDIVERT_DIR}/x64/WinDivert64.sys" WINDIVERT_SYS_PATH)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/resources.rc"
    @ONLY
)

add_executable(ProxyManSetup
    src/installer.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/resources.rc"
)
set_target_properties(ProxyManSetup PROPERTIES OUTPUT_NAME "ProxyManSetup")
add_dependencies(ProxyManSetup ProxyMan)

# ============================================================
# Complete System Uninstaller Utility: ProxyManUninstall.exe
# ============================================================
add_executable(ProxyManUninstall
    src/uninstaller.cpp
)
set_target_properties(ProxyManUninstall PROPERTIES OUTPUT_NAME "ProxyManUninstall")

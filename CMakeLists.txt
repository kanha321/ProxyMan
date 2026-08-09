cmake_minimum_required(VERSION 3.15)
project(ProxyMan CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O1 /GL /Gy /Gw /EHsc)
    add_link_options(/LTCG /OPT:REF /OPT:ICF)
endif()

set(WINDIVERT_DIR "${CMAKE_SOURCE_DIR}/vendor/WinDivert")

include_directories(
    src
    "${WINDIVERT_DIR}/include"
)

# ============================================================
# ProxyMan.exe — Main Proxy Engine
# ============================================================
add_executable(ProxyMan WIN32
    # Entry point
    src/main.cpp

    # CLI command handlers
    src/cli/parser.cpp
    src/cli/help.cpp
    src/cli/status.cpp
    src/cli/stop.cpp
    src/cli/logs.cpp
    src/cli/stats.cpp
    src/cli/diagnostics.cpp
    src/cli/autostart.cpp

    # Core proxy engine
    src/core/engine.cpp
    src/core/relay.cpp
    src/core/http_tunnel.cpp

    # Network interception
    src/net/packet_engine.cpp
    src/net/quic_blocker.cpp
    src/net/dns_interceptor.cpp
    src/net/network_watcher.cpp

    # Windows platform integration
    src/platform/elevation.cpp
    src/platform/ipc.cpp
    src/platform/console.cpp
    src/platform/proxy_settings.cpp
    src/platform/process_utils.cpp
    src/platform/wifi_utils.cpp

    # Configuration
    src/config/config.cpp
    src/config/pool.cpp

    # Logging
    src/logging/data_tracker.cpp
)

set_target_properties(ProxyMan PROPERTIES OUTPUT_NAME "ProxyMan")
target_link_libraries(ProxyMan PRIVATE
    ws2_32 iphlpapi wininet wlanapi psapi
    "${WINDIVERT_DIR}/x64/WinDivert.lib"
)

# Copy WinDivert driver files next to the executable
add_custom_command(TARGET ProxyMan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WINDIVERT_DIR}/x64/WinDivert.dll"
        "$<TARGET_FILE_DIR:ProxyMan>/WinDivert.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WINDIVERT_DIR}/x64/WinDivert64.sys"
        "$<TARGET_FILE_DIR:ProxyMan>/WinDivert64.sys"
)

# ============================================================
# ProxyManUninstall.exe — Complete System Uninstaller
# ============================================================
add_executable(ProxyManUninstall
    src/installer/uninstaller.cpp
)
set_target_properties(ProxyManUninstall PROPERTIES OUTPUT_NAME "ProxyManUninstall")

# ============================================================
# ProxyManSetup.exe — Self-Contained Installer
# Embeds ProxyMan.exe, ProxyManUninstall.exe, WinDivert.dll, WinDivert64.sys
# ============================================================
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/Release/ProxyMan.exe" PROXYMAN_EXE_PATH)
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/Release/ProxyManUninstall.exe" PROXYMAN_UNINSTALL_EXE_PATH)
file(TO_CMAKE_PATH "${WINDIVERT_DIR}/x64/WinDivert.dll" WINDIVERT_DLL_PATH)
file(TO_CMAKE_PATH "${WINDIVERT_DIR}/x64/WinDivert64.sys" WINDIVERT_SYS_PATH)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/installer/resources.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/resources.rc"
    @ONLY
)

add_executable(ProxyManSetup
    src/installer/installer.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/resources.rc"
)
set_target_properties(ProxyManSetup PROPERTIES OUTPUT_NAME "ProxyManSetup")
target_include_directories(ProxyManSetup PRIVATE "${CMAKE_SOURCE_DIR}/src/installer")
add_dependencies(ProxyManSetup ProxyMan ProxyManUninstall)

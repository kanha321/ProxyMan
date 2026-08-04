cmake_minimum_required(VERSION 3.15)
project(ProxyMan CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WINDIVERT_DIR "${CMAKE_SOURCE_DIR}/../WinDivert-2.2.2-A")

include_directories(
    src
    "${WINDIVERT_DIR}/include"
)

# ============================================================
# Production Binary: ProxyMan.exe
# ============================================================
add_executable(ProxyMan
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
)
set_target_properties(ProxyMan PROPERTIES OUTPUT_NAME "ProxyMan")
target_link_libraries(ProxyMan PRIVATE ws2_32 iphlpapi wininet "${WINDIVERT_DIR}/x64/WinDivert.lib")

# Copy WinDivert driver files for ProxyMan
add_custom_command(TARGET ProxyMan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WINDIVERT_DIR}/x64/WinDivert.dll"
        "$<TARGET_FILE_DIR:ProxyMan>/WinDivert.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WINDIVERT_DIR}/x64/WinDivert64.sys"
        "$<TARGET_FILE_DIR:ProxyMan>/WinDivert64.sys"
)

# ============================================================
# Interactive CLI/TUI Setup Installer: ProxyManSetup.exe
# ============================================================
add_executable(ProxyManSetup src/installer.cpp)
set_target_properties(ProxyManSetup PROPERTIES OUTPUT_NAME "ProxyManSetup")

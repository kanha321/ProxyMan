cmake_minimum_required(VERSION 3.15)
project(net_proxy_toggle CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(net-proxy-toggle
    src/main.cpp
    src/network_watcher.cpp
    src/network_watcher.h
    src/proxy_settings.cpp
    src/proxy_settings.h
)

target_link_libraries(net-proxy-toggle PRIVATE iphlpapi ws2_32 wininet)

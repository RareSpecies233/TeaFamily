cmake_minimum_required(VERSION 3.16)
project(tea-plugin-monitor-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(tea-plugin-monitor-client src/main.cpp)
target_link_libraries(tea-plugin-monitor-client PRIVATE tea_common nlohmann_json::nlohmann_json spdlog::spdlog)
target_include_directories(tea-plugin-monitor-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

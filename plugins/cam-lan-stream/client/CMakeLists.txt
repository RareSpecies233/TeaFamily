cmake_minimum_required(VERSION 3.16)
project(tea-plugin-cam-lan-stream-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(tea-plugin-cam-lan-stream-client src/main.cpp)
target_link_libraries(tea-plugin-cam-lan-stream-client PRIVATE
    tea_common
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
target_include_directories(tea-plugin-cam-lan-stream-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
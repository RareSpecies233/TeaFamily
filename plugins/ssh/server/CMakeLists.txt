cmake_minimum_required(VERSION 3.16)
project(tea-plugin-ssh-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(tea-plugin-ssh-server src/main.cpp)
target_link_libraries(tea-plugin-ssh-server PRIVATE tea_common nlohmann_json::nlohmann_json spdlog::spdlog)
target_include_directories(tea-plugin-ssh-server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

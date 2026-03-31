cmake_minimum_required(VERSION 3.16)
project(tea-plugin-ssh-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(tea-plugin-ssh-client src/main.cpp)
target_link_libraries(tea-plugin-ssh-client PRIVATE tea_common nlohmann_json::nlohmann_json spdlog::spdlog)
target_include_directories(tea-plugin-ssh-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

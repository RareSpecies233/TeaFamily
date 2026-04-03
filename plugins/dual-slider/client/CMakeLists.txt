cmake_minimum_required(VERSION 3.16)
project(tea-plugin-dual-slider-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(tea-plugin-dual-slider-client src/main.cpp)
target_link_libraries(tea-plugin-dual-slider-client PRIVATE tea_common nlohmann_json::nlohmann_json spdlog::spdlog)
target_include_directories(tea-plugin-dual-slider-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

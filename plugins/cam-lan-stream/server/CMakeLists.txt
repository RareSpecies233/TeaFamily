cmake_minimum_required(VERSION 3.16)
project(tea-plugin-cam-lan-stream-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(tea-plugin-cam-lan-stream-server src/main.cpp)
target_link_libraries(tea-plugin-cam-lan-stream-server PRIVATE
    tea_common
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    httplib::httplib
)
target_include_directories(tea-plugin-cam-lan-stream-server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(DEFINED httplib_SOURCE_DIR AND NOT "${httplib_SOURCE_DIR}" STREQUAL "")
    target_include_directories(tea-plugin-cam-lan-stream-server BEFORE PRIVATE "${httplib_SOURCE_DIR}")
    target_compile_definitions(tea-plugin-cam-lan-stream-server PRIVATE TEA_HTTPLIB_HEADER_FILE="${httplib_SOURCE_DIR}/httplib.h")
endif()

if(DEFINED Brotli_INCLUDE_DIR AND NOT "${Brotli_INCLUDE_DIR}" STREQUAL "")
    target_include_directories(tea-plugin-cam-lan-stream-server PRIVATE "${Brotli_INCLUDE_DIR}")
endif()

if(TARGET Brotli::decoder AND TARGET Brotli::encoder AND TARGET Brotli::common)
    target_link_libraries(tea-plugin-cam-lan-stream-server PRIVATE Brotli::decoder Brotli::encoder Brotli::common)
endif()

cmake_minimum_required(VERSION 3.16)
project(TeaFamily VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Platform detection
if(APPLE)
    add_definitions(-DPLATFORM_MACOS)
elseif(UNIX)
    add_definitions(-DPLATFORM_LINUX)
endif()

# Fetch dependencies
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(nlohmann_json httplib spdlog)

# Subdirectories
add_subdirectory(common)
add_subdirectory(GreenTea)
add_subdirectory(HoneyTea)
add_subdirectory(LemonTea)
add_subdirectory(plugins)

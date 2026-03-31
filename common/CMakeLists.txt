add_library(tea_common STATIC
    src/protocol.cpp
    src/tcp_connection.cpp
    src/tcp_server.cpp
    src/udp_socket.cpp
    src/process_manager.cpp
)

target_include_directories(tea_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tea_common PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

if(APPLE)
    target_link_libraries(tea_common PUBLIC "-framework CoreFoundation" "-framework IOKit")
endif()

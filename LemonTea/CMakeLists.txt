add_executable(LemonTea
    src/main.cpp
    src/lemon_tea.cpp
    src/http_server.cpp
    src/plugin_manager.cpp
)

target_link_libraries(LemonTea PRIVATE tea_common httplib::httplib)
target_include_directories(LemonTea PRIVATE src)

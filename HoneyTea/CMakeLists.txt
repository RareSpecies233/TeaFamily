add_executable(HoneyTea
    src/main.cpp
    src/honey_tea.cpp
    src/plugin_manager.cpp
)

target_link_libraries(HoneyTea PRIVATE tea_common)
target_include_directories(HoneyTea PRIVATE src)

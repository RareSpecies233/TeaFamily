add_executable(GreenTea
    src/main.cpp
    src/watchdog.cpp
    src/updater.cpp
)

target_link_libraries(GreenTea PRIVATE tea_common)
target_include_directories(GreenTea PRIVATE src)

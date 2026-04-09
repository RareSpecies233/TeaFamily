# Plugins CMakeLists
# Each plugin has server/ and client/ sub-projects

add_subdirectory(ssh/server)
add_subdirectory(ssh/client)
add_subdirectory(file-manager/server)
add_subdirectory(file-manager/client)
add_subdirectory(monitor/server)
add_subdirectory(monitor/client)
add_subdirectory(dual-slider/server)
add_subdirectory(dual-slider/client)
add_subdirectory(cam-lan-stream/server)
add_subdirectory(cam-lan-stream/client)

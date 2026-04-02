add_executable(LemonTea
    src/main.cpp
    src/lemon_tea.cpp
    src/http_server.cpp
    src/plugin_manager.cpp
)

target_link_libraries(LemonTea PRIVATE tea_common httplib::httplib)
target_include_directories(LemonTea PRIVATE src)

if(DEFINED httplib_SOURCE_DIR AND NOT "${httplib_SOURCE_DIR}" STREQUAL "")
    target_include_directories(LemonTea BEFORE PRIVATE "${httplib_SOURCE_DIR}")
    target_compile_definitions(LemonTea PRIVATE TEA_HTTPLIB_HEADER_FILE="${httplib_SOURCE_DIR}/httplib.h")
endif()

# Some cross-toolchain Brotli find modules do not propagate include dirs via imported targets.
# Keep LemonTea robust when httplib enables CPPHTTPLIB_BROTLI_SUPPORT.
if(DEFINED Brotli_INCLUDE_DIR AND NOT "${Brotli_INCLUDE_DIR}" STREQUAL "")
    target_include_directories(LemonTea PRIVATE "${Brotli_INCLUDE_DIR}")
endif()

# For static Brotli builds, link order matters. Re-append in dependency order.
if(TARGET Brotli::decoder AND TARGET Brotli::encoder AND TARGET Brotli::common)
    target_link_libraries(LemonTea PRIVATE Brotli::decoder Brotli::encoder Brotli::common)
endif()

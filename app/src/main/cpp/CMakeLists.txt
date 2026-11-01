cmake_minimum_required(VERSION 3.12)

include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/opus/include
)

# Kingmakers RP server endpoint (see samp/servers.h)
if(DEFINED KM_SERVER_HOST)
    add_compile_definitions(KM_SERVER_HOST="${KM_SERVER_HOST}")
endif()
if(DEFINED KM_SERVER_PORT)
    add_compile_definitions(KM_SERVER_PORT=${KM_SERVER_PORT})
endif()

find_package(shadowhook REQUIRED CONFIG)
add_subdirectory(opus)
add_subdirectory(samp)

target_compile_options(opus PRIVATE -O3)


# Logs and crash reports for phones whose logcat apps can't read (kmdiag/kmdiag.c).
add_library(kmdiag SHARED kmdiag/kmdiag.c)
target_link_libraries(kmdiag log dl shadowhook::shadowhook)

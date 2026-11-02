cmake_minimum_required(VERSION 3.12)

include_directories(
        ${CMAKE_CURRENT_LIST_DIR}/opus/include
)

# Kingmakers RP server endpoint (see samp/servers.h)
if(DEFINED KM_SERVER_HOST)
    add_compile_definitions(KM_SERVER_HOST="${KM_SERVER_HOST}")
endif()
if(DEFINED KM_CONTENT_PORT)
    add_compile_definitions(KM_CONTENT_PORT=${KM_CONTENT_PORT})
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

# Patches the GTA:SA 2.11 engine needs in the launcher (km211/km211.c).
add_library(km211 SHARED km211/km211.c)
target_link_libraries(km211 log shadowhook::shadowhook)

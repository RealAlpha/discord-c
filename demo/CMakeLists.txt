cmake_minimum_required(VERSION 3.5)
project(demo)


### START OF DISCORD-C DEPENDENCIES
find_package(cJSON REQUIRED)
find_package(libwebsockets REQUIRED)
find_package(CURL REQUIRED)

# Ensure the CJSON directories can be included
include_directories(${CJSON_INCLUDE_DIR})

# Based on: https://gitlab.indel.ch/thirdparty/libwebsockets/-/blob/master/scripts/FindLibWebSockets.cmake
FIND_PATH ( LIBWEBSOCKETS_INCLUDE_DIR libwebsockets.h
    /usr/local/include
    /usr/include
)
include_directories(${LIBWEBSOCKETS_INCLUDE_DIR})

TARGET_LINK_LIBRARIES (discord-c ${CJSON_LIBS})
### END OF DEPENDENCIES

include_directories(../lib)

add_executable(demo main.c)

#find_package(discord-c REQUIRED)
target_link_libraries(demo LINK_PUBLIC discord-c)
#target_link_libraries(demo LINK_PUBLIC lib)



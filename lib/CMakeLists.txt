cmake_minimum_required(VERSION 3.5)
project(discord-c)

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

add_library (discord-c discord-c.c)


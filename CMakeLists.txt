cmake_minimum_required(VERSION 3.13)
project(Nettverksprog_prosjekt)

find_path(EVENT_INCLUDE_DIR event.h
        PATHS /usr/include/x86_64-linux-gnu
        PATH_SUFFIXES event2)

find_library(EVENT_LIBRARY
        NAMES event
        PATHS /usr/lib/x86_64-linux-gnu)
set(EVENT_LIBRARIES ${EVENT_LIBRARY} )
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EVENT
        DEFAULT_MSG
        EVENT_INCLUDE_DIR
        EVENT_LIBRARIES
        )

find_path(NGHTTP2_INCLUDE_DIR "nghttp2/nghttp2.h")

find_library(NGHTTP2_LIBRARY NAMES nghttp2)

find_package_handle_standard_args(NGHTTP2
        FOUND_VAR
        NGHTTP2_FOUND
        REQUIRED_VARS
        NGHTTP2_LIBRARY
        NGHTTP2_INCLUDE_DIR
        FAIL_MESSAGE
        "Could NOT find NGHTTP2"
        )

set(NGHTTP2_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIR})
set(NGHTTP2_LIBRARIES ${NGHTTP2_LIBRARY})



set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pthread -Wall -Wextra -Wno-unused-parameter")
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(Nettverksprog_prosjekt main.cpp frames.hpp)
target_link_libraries(Nettverksprog_prosjekt ${OPENSSL_LIBRARIES} -levent_openssl -levent ${EVENT_LIBRARIES} ${NGHTTP2_LIBRARIES})

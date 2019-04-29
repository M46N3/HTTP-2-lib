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

find_package(Boost 1.68.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pthread -Wall -Wextra -Wno-unused-parameter")
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(HTTP-2-lib-server main.cpp src/h2_server.cpp src/h2_callbacks.cpp src/h2_utils.cpp src/h2_frame_handlers.cpp src/h2_config.cpp src/h2_structs.hpp src/h2_enums.hpp)
target_link_libraries(HTTP-2-lib-server ${OPENSSL_LIBRARIES} -levent_openssl -levent ${EVENT_LIBRARIES} ${NGHTTP2_LIBRARIES})

include_directories(${Boost_INCLUDE_DIR})
add_executable(Tests tests/main.test.cpp tests/h2_utils.test.cpp tests/h2_server.test.cpp src/h2_server.cpp src/h2_callbacks.cpp src/h2_utils.cpp src/h2_frame_handlers.cpp
        src/h2_config.cpp src/h2_structs.hpp src/h2_enums.hpp)
target_link_libraries(Tests ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} -levent_openssl -levent ${EVENT_LIBRARIES} ${NGHTTP2_LIBRARIES})
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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pthread -Wall -Wextra -Wno-unused-parameter")
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(Nettverksprog_prosjekt main.cpp)
target_link_libraries(Nettverksprog_prosjekt ${OPENSSL_LIBRARIES} -levent_openssl -levent ${EVENT_LIBRARIES})
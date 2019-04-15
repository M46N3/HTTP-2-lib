cmake_minimum_required(VERSION 3.13)
project(Nettverksprog_prosjekt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pthread -Wall -Wextra -Wno-unused-parameter")

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(Nettverksprog_prosjekt main.cpp frames.hpp)
target_link_libraries(Nettverksprog_prosjekt ${OPENSSL_LIBRARIES})
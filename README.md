<p align="center">
  <a href="https://github.com/M46N3/HTTP-2-lib/">
    <img src="http://i66.tinypic.com/2dkhfzt.png" alt="HTTP-2-lib logo" width="100" height="100">
  </a>
</p>

<h3 align="center">HTTP-2-lib</h3>

<p align="center">
  More efficient use of network resources, reduced perception of latency and multiple concurrent exchanges on the same connection.
</p>

## Table of contents

- [Introduction](#introduction)
- [What's included](#whats-included)
- [Implemented functionality](#implemented-functionality)
- [Future work](#future-work)
- [External information](#external-information)



## Introduction
This library is an implementation of the [RFC 7540](https://tools.ietf.org/html/rfc7540) - Hypertext Transfer Protocol Version 2 (HTTP/2) in c++.


## What's included

Within the download you'll find the following directories and files, logically grouping common assets and providing both compiled and minified variations. You'll see something like this:

```text
HTTP-2-lib/
├── cmake-build-debug/
|   ├── CMakeFiles/
|   ├── cmake_install.cmake
|   ├── CMakeChache.txt
|   ├── HTTP-2-lib-server
|   ├── Makefile
|   ├── report
|   └── Tests
|
├── include/
|   └── HTTP-2-lib/
|       └── h2_server.hpp
|
├── public/
|   ├── index.html
|   ├── index2.html
|   ├── script.js
|   └── style.css
|   
├── src/
|   ├── h2_callbacks.cpp
|   ├── h2_callback.hpp
|   ├── h2_config.cpp
|   ├── h2_config.hpp
|   ├── h2_enums.hpp
|   ├── h2_frame_handlers.cpp
|   ├── h2_frame_handlers.hpp
|   ├── h2_global.hpp
|   ├── h2_server.cpp
|   ├── h2_structs.hpp
|   ├── h2_utils.cpp
|   └── h2_utils.hpp
│
├── tests/
|    └── h2_utils.test.cpp
|
├── cert.pem
├── CMakeLists.txt
├── main.cpp
├── key.pem
└── README.md

```

## Implemented functionality
We have implemented..


## Future work


## External information
- [RFC 7540 - Hypertext Transfer Protocol Version 2 (HTTP/2)](https://tools.ietf.org/html/rfc7540)
- [RFC 7541 - HPACK: Header Compression for HTTP/2](https://tools.ietf.org/html/rfc7541)
- [RFC 7301 - Transport Layer Security (TLS), Application-Layer Protocol Negotiation Extension](https://tools.ietf.org/html/rfc7301)
- [Tutorial:HTTP/2 server](https://nghttp2.org/documentation/tutorial-server.html#)
- [Tutorial:HPACK API](https://nghttp2.org/documentation/tutorial-hpack.html)
- Learning HTTP/2 a practical guide for beginners

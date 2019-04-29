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
- [Prerequisites](#prerequisites)
- [Installing dependencies](#installing-dependencies)
- [Compiling and running](#compiling-and-running)
- [Short introduction to HTTP2](#short-introduction-to-HTTP2)
- [What's included](#whats-included)
- [Implemented functionality](#implemented-functionality)
- [Future work](#future-work)
- [External information](#external-information)



## Introduction
HTTP-2-lib is an implementation of the [RFC 7540](https://tools.ietf.org/html/rfc7540) - Hypertext Transfer Protocol Version 2 (HTTP/2) in C++. It is important to notice that this library is a work in progress, so there will be some missing HTTP/2-functionalities.


## Prerequisites
* Linux
* [C++ compiler](https://gcc.gnu.org/)

## Installing dependencies
Dependencies needed to use HTTP-2-lib:
* [Boost](https://github.com/boostorg/boost): Used for unit testing.
* [libevent](https://github.com/libevent/libevent): Used for asynchronous buffer read and write.
* [nghttp2](https://github.com/nghttp2/nghttp2): HPACK to compress and decompress header frames.
* [OpenSSL](https://github.com/openssl/openssl): TLS and ALPN support.


## Compiling and running
Text..
```sh
git clone https://github.com/M46N3/HTTP-2-lib.git
```

## Short introduction to HTTP2
HTTP/2 has the same purpose as earlier versions of HTTP: to provide a standard way for web browsers and servers to talk to each other. The HTTP-protocol is in the application layer in the OSI-model. HTTP/2 provides an optimized transport for HTTP semantics and aims to be more efficient than earlier version of HTTP.

HTTP/2 enables more efficient processing of messages with binary message framing. The original request response messages are divided into header-frames and data-frames. Header-frames is the same as headers in earlier versions, and data-frames is the same as body, but is sent as individually frames on the same stream.

Head-of-line-blocking have been a problem in earlier versions of HTTP, HTTP/2 introduced multiplexing, which reduced the amount of head-of-line-blocking. Multiplexing of requests is achieved by each having HTTP request/response exchange associated with its own stream. Streams are largely independent of each other, so a blocked or stalled request or response does not prevent progress on other streams.
Flow control and prioritization is also important for HTTP/2, these ensure that it is possible to efficiently use multiplexed streams. 
Server push is a new feature in HTTP/2 to make request/response more efficient.

Server push allows a server to speculatively send data to a client that the server anticipates the client will need. For example, if the client is requesting index.html, the server push style.css and script.js to the client, because it predict that it will need it soon.

In earlier version of HTTP we did not compress header fields, HTTP/2 is the first version to introduce header field compression. Because HTTP header fields used in a connection can contain large amounts of redundant data, we sent unnecessary amount of data instead of compressing it and reduce the amount of data sent over the connection.



## What's included

Within the download you'll find the following directories and files:

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
- Book: [O'Reilly Media, Learning HTTP/2, A practical guide for beginners](https://www.amazon.com/Learning-HTTP-Practical-Guide-Beginners/dp/1491962445)

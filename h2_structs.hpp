// h2_structs.hpp

#pragma once
#include <openssl/ssl.h>
#include <nghttp2/nghttp2.h>
#include <unordered_map>
#include <string>

struct ApplicationContext {
    SSL_CTX *ctx;
    struct event_base *eventBase;
    std::unordered_map<std::string, std::string> routes;
};

struct ClientSessionData {
    //struct stream_data root;
    struct bufferevent *bufferEvent;
    struct ApplicationContext *appCtx;
    //h2_session *session;
    char *clientAddress;
    nghttp2_hd_inflater *inflater;
    nghttp2_hd_deflater *deflater;
};
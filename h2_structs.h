#ifndef NETTVERKSPROG_PROSJEKT_H2_STRUCTS_H
#define NETTVERKSPROG_PROSJEKT_H2_STRUCTS_H
#include <openssl/ssl.h>

struct ApplicationContext {
    SSL_CTX *ctx;
    struct event_base *eventBase;
};

struct ClientSessionData {
    struct stream_data root;
    struct bufferevent *bufferEvent;
    struct ApplicationContext *appCtx;
    //h2_session *session;
    char *clientAddress;
};
#endif //NETTVERKSPROG_PROSJEKT_H2_STRUCTS_H

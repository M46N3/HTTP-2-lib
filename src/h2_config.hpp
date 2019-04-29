// h2_config.hpp

#pragma once
#include <openssl/ssl.h>
#include <unordered_map>
#include <string>

class h2_config {
public:
    static int nextProtocolCallback(SSL *s, const unsigned char **out,
                                    unsigned int *outlen, void *arg);
    static int selectProtocol(unsigned char **out, unsigned char *outlen,
                              const unsigned char *in, unsigned int inlen);
    static int alpnSelectProtocolCallback(SSL *ssl, const unsigned char **out,
                                              unsigned char *outlength, const unsigned char *in,
                                              unsigned int inlen, void *arg);
    static void configureAlpn(SSL_CTX *ctx);
    static void createApplicationContext(struct ApplicationContext *appCtx, SSL_CTX *sslCtx, struct event_base *eventBase_, std::unordered_map<std::string, std::string> routes);
    static SSL_CTX *createSslContext();
    static void configureContext(SSL_CTX *ctx, const char *certKeyFile, const char *certFile);
    static void initOpenssl();
    static void cleanupOpenssl();
};
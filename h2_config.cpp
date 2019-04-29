// h2_config.cpp

#include "h2_config.hpp"
#include "h2_structs.hpp"
#include "h2_global.hpp"
#include <iostream>
#include <string.h>
#include <openssl/err.h>
#include "h2_global.hpp"

using namespace std;

static unsigned char next_proto_list[256];
static size_t next_proto_list_len;


/// nextProtocolCallback - Callback function used when the TLS server needs a list of supported protocols
///                        for Next Protocol Negotiation.
///
/// @param s - SSL object (unused)
/// @param out - pointer to list of supported protocols in protocol-list format.
/// @param outlen - length of 'out'.
/// @param arg - user supplied context (unused).
/// @return int - SSL_TLSEXT_ERR_OK

int h2_config::nextProtocolCallback(SSL *s, const unsigned char **out,
                                unsigned int *outlen, void *arg) {
    if (printTrackers){
        cout << "[ nextProtocolCallback ]" << endl;
    }

    *out = next_proto_list;
    *outlen = (unsigned int)next_proto_list_len;
    return SSL_TLSEXT_ERR_OK;
}


/// selectProtocol - Selects 'h2' meaning 'HTTP/2 over TLS'
///
/// @param out - name of protocol chosen by the server.
/// @param outlen - length of name of protocol given in 'out'.
/// @param in  - string of protocols supported by the client, prefixed by the length of the following protocol.
/// @param inlen - total length of the protocols-string 'in'.
/// @return - 1 if successful.

int h2_config::selectProtocol(unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen) {

    if (printTrackers) {
        cout << "[ selectProtocol ]" << endl;
    }

    unsigned int start_index = (unsigned int) in[0];
    unsigned int end_of_next_protocol = start_index ;

    unsigned int i = 0;

    if (printComments) {
        cout << endl << "Client supports following protocols, ordered according to preference: " << endl;
        while (i < inlen) {
            while (i <= end_of_next_protocol && i < inlen) {
                cout << in[i];
                ++i;
            }
            cout << endl;
            end_of_next_protocol += in[i] + 1;
            ++i;

        }
        cout << endl;
    }
    *outlen = in[0];
    out[0] = (unsigned char*) "h2";
    return 1;
}


/// alpnSelectProtocolCallback - Callback function used for the negotiation of HTTP/2 over TLS in ALPN.
///
/// @param ssl - SSL object (unused parameter).
/// @param out - string for chosen protocol
/// @param outlength - length of name of chosen protocol given in 'out'.
/// @param in - protocols supported by the client.
/// @param inlen - length of 'in'.
/// @param arg
/// @return - returns 0 on success and non-zero on failure.

int h2_config::alpnSelectProtocolCallback(SSL *ssl, const unsigned char **out,
                                      unsigned char *outlength, const unsigned char *in,
                                      unsigned int inlen, void *arg) {

    if (printTrackers) {
        cout << "[ alpnSelectProtocolCallback ]" << endl;
    }

    int rv;

    rv = selectProtocol((unsigned char **) out, outlength, in, inlen);

    if (rv != 1) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    if (printComments) {
        cout << "Server has chosen 'h2', meaning HTTP/2 over TLS" << endl;
    }

    return SSL_TLSEXT_ERR_OK;
}

/// configureAlpn - Configures the referenced SSL_CTX object to use the Aplication Protocol Layer Negotiation
/// extension, with 'nextProtocolCallback' and 'alpnSelectProtocolCallback'.
///
/// @param ctx - reference to the SSL_CTX object to configure ALPN extension on.

void h2_config::configureAlpn(SSL_CTX *ctx) {
    if (printTrackers) {
        cout << "[ configureAlpn ]" << endl;
    }

    next_proto_list[0] = 2;
    memcpy(&next_proto_list[1], "h2", 2);
    next_proto_list_len = 1 + 2;

    SSL_CTX_set_next_protos_advertised_cb(ctx, nextProtocolCallback, NULL);
    SSL_CTX_set_alpn_select_cb(ctx, alpnSelectProtocolCallback, NULL);
}


/// createApplicationContext - Initializes the application wide application_ctx object on the refernece given.
///
/// @param appCtx - reference to appCtx object to initialize.
/// @param sslCtx - SSL_CTX object to use.
/// @param eventBase_ - event_base object to use.

void h2_config::createApplicationContext(ApplicationContext *appCtx, SSL_CTX *sslCtx, struct event_base *eventBase_, std::unordered_map<std::string, std::string> routes) {
    /**
     * Sets the application_ctx members, ctx and eventBase, to the given SSL_CTX and event_base objects
     */

    if (printTrackers) {
        cout << "[ createApplicationContext ]" << endl;
    }

    memset(appCtx, 0, sizeof(ApplicationContext));
    appCtx->ctx = sslCtx;
    appCtx->eventBase = eventBase_;
    appCtx->routes = std::move(routes);
}


/// createSslContext - Creates a new SSL_CTX object and sets the connection method to be used,
/// which is a general SSL/TLS server connection method.
///
/// return ctx - The newly created SSL_CTX object with the connection method set.

SSL_CTX *h2_config::createSslContext() {
    if (printTrackers) {
        cout << "[ createSslContext ]" << endl;
    }

    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}


/// configureContext - Configures the SSL_CTX object to use given certificate and private key,
/// and calls to configure ALPN extension.
///
/// @param ctx - reference to the SSL_CTX object to configure.
/// @param certKeyFile - path to Certificate Key File to be used for TLS.
/// @param certFile - path to Certificate File to be used for TLS.

void h2_config::configureContext(SSL_CTX *ctx, const char *certKeyFile, const char *certFile) {
    if (printTrackers) {
        cout << "[ configureContext ]" << endl;
    }

    // Make server always choose the most appropriate curve for the client.
    SSL_CTX_set_ecdh_auto(ctx, 1);

    // Set the key and cert
    if (SSL_CTX_use_certificate_file(ctx, certFile, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, certKeyFile, SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Configure SSL_CTX object to use ALPN.
    configureAlpn(ctx);
}

void h2_config::initOpenssl() {
    if (printTrackers) {
        cout << "[ initOpenssl ]" << endl;
    }
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_ssl_algorithms();
}
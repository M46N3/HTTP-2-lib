#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
#include <event.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent_ssl.h>
#include <signal.h>
#include <err.h>
#include <netinet/tcp.h>
#include "h2_frames.hpp"
#include <nghttp2/nghttp2.h>
#include <sstream>
#include <fstream>
#include "h2_callbacks.hpp"
#include "h2_structs.hpp"

using namespace std;

static unsigned char next_proto_list[256];
static size_t next_proto_list_len;
static bool printComments = true; // Turn off comments.
static bool printTrackers = true; // boolean to turn on/off printing of tracker-comments.
static bool printFrames = true; // boolean to turn on/off printing of frames.


void initOpenssl() {

    if (printTrackers) {
        cout << "[ initOpenssl ]" << endl;
    }

    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}


/// createSslContext - Creates a new SSL_CTX object and sets the connection method to be used,
/// which is a general SSL/TLS server connection method.
///
/// return ctx - The newly created SSL_CTX object with the connection method set.

SSL_CTX *createSslContext() {

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


/// nextProtocolCallback -
///
/// \param s
/// \param data
/// \param len
/// \param arg
/// \return

static int nextProtocolCallback(SSL *s, const unsigned char **data,
                                unsigned int *len, void *arg) {

    if (printTrackers) {
        cout << "[ nextProtocolCallback ]" << endl;
    }

    *data = next_proto_list;
    *len = (unsigned int)next_proto_list_len;
    return SSL_TLSEXT_ERR_OK;
}


/// selectProtocol - Selects 'h2' meaning 'HTTP/2 over TLS'
///
/// @param out - name of protocol chosen by the server.
/// @param outlen - length of name of protocol given in 'out'.
/// @param in  - string of protocols supported by the client, prefixed by the length of the following protocol.
/// @param inlen - total length of the protocols-string 'in'.
/// @return - 1 if successful.

static int selectProtocol(unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen) {

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

static int alpnSelectProtocolCallback(SSL *ssl, const unsigned char **out,
                                      unsigned char *outlength, const unsigned char *in,
                                      unsigned int inlen, void *arg) {

    if (printTrackers) {
        cout << "[ alpnSelectProtocolCallback ]" << endl;
    }

    int returnValue;

    returnValue = selectProtocol((unsigned char **) out, outlength, in, inlen);

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

void configureAlpn(SSL_CTX *ctx) {
    if (printTrackers) {
        cout << "[ configureAlpn ]" << endl;
    }

    next_proto_list[0] = 2;
    memcpy(&next_proto_list[1], "h2", 2);
    next_proto_list_len = 1 + 2;

    SSL_CTX_set_next_protos_advertised_cb(ctx, nextProtocolCallback, NULL);

    SSL_CTX_set_alpn_select_cb(ctx, alpnSelectProtocolCallback, NULL);
}


/// configureContext - Configures the SSL_CTX object to use given certificate and private key,
/// and calls to configure ALPN extension.
///
/// @param ctx - reference to the SSL_CTX object to configure.
/// @param certKeyFile - path to Certificate Key File to be used for TLS.
/// @param certFile - path to Certificate File to be used for TLS.

void configureContext(SSL_CTX *ctx, const char *certKeyFile, const char *certFile) {
    if (printTrackers) {
        cout << "[ configureContext ]" << endl;
    }

    /* Make server always choose the most appropriate curve for the client. */
    SSL_CTX_set_ecdh_auto(ctx, 1);

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, certFile, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, certKeyFile, SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* Configure SSL_CTX object to use ALPN */
    configureAlpn(ctx);
}

struct stream_data {
    struct stream_data *prev, *next;
    char *requested_path;
    int32_t stream_id;
    int sock;
} stream_data;



/// createApplicationContext - Initializes the application wide application_ctx object on the refernece given.
///
/// @param appCtx - reference to appCtx object to initialize.
/// @param sslCtx - SSL_CTX object to use.
/// @param eventBase_ - event_base object to use.

static void createApplicationContext(ApplicationContext *appCtx, SSL_CTX *sslCtx, struct event_base *eventBase_) {
    /**
     * Sets the application_ctx members, ctx and eventBase, to the given SSL_CTX and event_base objects
     */

    if (printTrackers) {
        cout << "[ createApplicationContext ]" << endl;
    }

    memset(appCtx, 0, sizeof(ApplicationContext));
    appCtx->ctx = sslCtx;
    appCtx->eventBase = eventBase_;
}

/// serverListen - Sets up the server and starts listening on the given port.
///
/// @param eventBase - The application-wide event_base object to use with listeners.
/// @param port - The port number the application should listen on.
/// @param appCtx - The global application context object to use.

static void serverListen(struct event_base *eventBase, const char *port, ApplicationContext *appCtx) {

    if (printTrackers) {
        cout << "[ serverListen ]" << endl;
    }

    int return_value;
    struct addrinfo hints;
    struct addrinfo *res, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
#ifdef AI_ADDRCONFIG
    hints.ai_flags |= AI_ADDRCONFIG;
#endif

    return_value = getaddrinfo(NULL, port, &hints, &res);
    if (return_value !=0) {
        printf("%s", "Error: Could not resolve server address");
    }

    for (rp = res; rp; rp = rp->ai_next) {
        struct evconnlistener *conListener;
        conListener = evconnlistener_new_bind(
                eventBase, h2_callbacks::acceptCallback, appCtx, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
                16, rp->ai_addr, (int) rp->ai_addrlen);
        if (conListener) {
            freeaddrinfo(res);
            return;
        }
    }

    // if for loop above does not return, starting the listener has failed
    printf("%s", "Error: Could not start listener");
}

static void run(const char *port, const char *certKeyFile, const char *certFile) {
    if (printTrackers) {
        cout << "[ run ]" << endl;
    }

    SSL_CTX *sslCtx;
    ApplicationContext appCtx;
    struct event_base *eventBase;

    sslCtx = createSslContext();
    configureContext(sslCtx, certKeyFile, certFile);
    eventBase = event_base_new();
    createApplicationContext(&appCtx, sslCtx, eventBase);

    serverListen(eventBase, port, &appCtx);

    event_base_loop(eventBase, 0);

    event_base_free(eventBase);
    SSL_CTX_free(sslCtx);
}

int main(int argc, char **argv) {
    struct sigaction act;

    if (argc < 4) {
        cerr << "http2-server PORT PRIVATE_KEY_FILE CERT_FILE\n" << endl;
        exit(EXIT_FAILURE);
    }

    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);

    initOpenssl();

    run(argv[1], argv[2], argv[3]);
    return 0;
}
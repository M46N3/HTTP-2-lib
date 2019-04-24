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
#include "frames.hpp"

using namespace std;

static unsigned char next_proto_list[256];
static size_t next_proto_list_len;

void init_openssl() {

    cout << "[ init_openssl ]" << endl;

    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX *create_ssl_context() {
    /**
     * Creates a new SSL_CTX object and sets the connection method
     * to be used, which is a general SSL/TLS server connection method.
     *
     * return: The new SSL_CTX object with connection method set.
     */

    cout << "[ create_ssl_context ]" << endl;

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

static int next_proto_cb(SSL *s, const unsigned char **data,
                         unsigned int *len, void *arg) {

    cout << "[ next_proto_cb ]" << endl;

    *data = next_proto_list;
    *len = (unsigned int)next_proto_list_len;
    return SSL_TLSEXT_ERR_OK;
}


static int select_protocol(unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen) {
    /**
     *
     */

    cout << "[ select_protocol ]" << endl;

    unsigned int start_index = (unsigned int) in[0];
    unsigned int end_of_next_protocol = start_index ;

    unsigned int i = 0;
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

    *outlen = in[0];
    out[0] = (unsigned char*) "h2";

    return 1;
}

static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
                                unsigned char *outlen, const unsigned char *in,
                                unsigned int inlen, void *arg) {

    cout << "[ alpn_select_proto_cb ]" << endl;

    int rv;

    rv = select_protocol((unsigned char **)out, outlen, in, inlen);

    if (rv != 1) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    cout << "Server has chosen 'h2', meaning HTTP/2 over TLS" << endl;

    return SSL_TLSEXT_ERR_OK;
}

void configure_alpn(SSL_CTX *ctx) {
    /**
     * Configures the given SSL_CTX object to use the Application Layer Protocol Negotiation extension,
     * with 'next protos advertised' and 'alpn select' callbacks.
     */

    cout << "[ configure_alpn ]" << endl;

    next_proto_list[0] = 2;
    memcpy(&next_proto_list[1], "h2", 2);
    next_proto_list_len = 1 + 2;

    SSL_CTX_set_next_protos_advertised_cb(ctx, next_proto_cb, NULL);

    SSL_CTX_set_alpn_select_cb(ctx, alpn_select_proto_cb, NULL);
}

void configure_context(SSL_CTX *ctx, const char *certKeyFile, const char *certFile) {
    /**
     * Configures the SSL_CTX object to use given certificate and private key,
     * and calls to configure ALPN extension.
     */

    cout << "[ configure_context ]" << endl;

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
    configure_alpn(ctx);
}

struct application_ctx {
    SSL_CTX *ctx;
    struct event_base *eventBase;
};

struct stream_data {
    struct stream_data *prev, *next;
    char *requested_path;
    int32_t stream_id;
    int sock;
} stream_data;

/* client_sess_data structure to store data pertaining to one h2 connection */
struct client_sess_data {
    struct stream_data root;
    struct bufferevent *bufferEvent;
    application_ctx *appCtx;
    //h2_session *session;
    char *clientAddress;
} client_session_data;

static client_sess_data *create_client_session_data(application_ctx *appCtx, int sock, struct sockaddr *clientAddress, int addressLength) {
    int returnValue;
    client_sess_data *clientSessData;
    SSL *ssl;
    char host[1025];
    int val = 1;

    /* Create new TLS session object */
    ssl = SSL_new(appCtx->ctx);
    if (!ssl) {
        errx(1, "Could not create TLS session object: %s",
                ERR_error_string(ERR_get_error(), NULL));
    }

    clientSessData = (client_sess_data *)malloc(sizeof(client_sess_data));
    memset(clientSessData, 0, sizeof(client_sess_data));

    clientSessData->appCtx = appCtx;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val)); // set TCP_NODELAY option for improved latency

    clientSessData->bufferEvent = bufferevent_openssl_socket_new(
            appCtx->eventBase, sock, ssl, BUFFEREVENT_SSL_ACCEPTING,
            BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS
            );


    bufferevent_enable(clientSessData->bufferEvent, EV_READ | EV_WRITE);

    returnValue = getnameinfo(clientAddress, (socklen_t)addressLength, host, sizeof(host), NULL, 0, NI_NUMERICHOST);

    if (returnValue != 0) {
        clientSessData->clientAddress = strdup("(unknown)");
    } else {
        clientSessData->clientAddress = strdup(host);
    }

    return clientSessData;

}


static void create_application_context(application_ctx *appCtx, SSL_CTX *sslCtx, struct event_base *eventBase_) {
    /**
     * Sets the application_ctx members, ctx and eventBase, to the given SSL_CTX and event_base objects
     */
    cout << "[ create_application_context ]" << endl;

    memset(appCtx, 0, sizeof(application_ctx));
    appCtx->ctx = sslCtx;
    appCtx->eventBase = eventBase_;
}

static void event_callback(struct bufferevent *bufferEvent, short events, void *ptr){
    client_sess_data *clientSessData = (client_sess_data *)ptr;


    if (events & BEV_EVENT_CONNECTED) {
        const unsigned char *alpn = NULL;
        unsigned int alpnlen = 0;

        SSL *ssl;
        (void)bufferEvent;

        printf( "%s connected\n", clientSessData->clientAddress);

        ssl = bufferevent_openssl_get_ssl(bufferEvent);

        /* Negotiate ALPN on initial connection */
        if (alpn == NULL) {
            SSL_get0_alpn_selected(ssl, &alpn, &alpnlen);
        }

        if (alpn == NULL || alpnlen != 2 || memcmp("h2", alpn, 2) != 0) {
            printf("%s h2 negotiation failed\n", clientSessData->clientAddress);
            /* TODO: delete_client_sess_data(clientSessData); */
            return;
        }

        // SETTINGS FRAME:
        bufferevent_write(bufferEvent, settingsframe(bitset<8>(0x0), bitset<31>(0x0)), 9);

        /*
        if(send_connection_header(clientSessData) != 0 ||
            client_sess_send(clientSessData) != 0) {
            // TODO: delete_client_sess_data(clientSessData);
            return;
        }

        return;
        */
    }
}

static int session_on_received(client_sess_data *clientSessData) {
    ssize_t readlen;
    struct evbuffer *in = bufferevent_get_input(clientSessData->bufferEvent);
    size_t length = evbuffer_get_length(in);
    unsigned char *data = evbuffer_pullup(in, -1); // Make whole buffer contiguous

    for (size_t i = 0; i < length; ++i) {
        printf("%02x ", data[i]);
    }
    cout << endl;

    if (data[3] == 0x04 && data[4] == 0x0) {
        // SETTINGS FRAME:
        bufferevent_write(clientSessData->bufferEvent, settingsframe(bitset<8>(0x01), bitset<31>(0x0)), 9);
        cout << "Settings frame recieved" << endl;
    }

    if (data[3] == 0x01) {
        string s = "<h1>Hello World!<h1>";
        auto sLen = s.length();
        // DATA FRAME:
        //cout << s << endl;
        //cout << sLen << endl;
        //cout << bitset<24>(sLen) << endl;

        //bufferevent_write(clientSessData->bufferEvent, dataframe(bitset<8>(0x1), bitset<31>(0x0), s, sLen), (sLen + 9));
    }

    readlen = 1;

    if (readlen < 0) {
        printf("Error: error storing recevied data in client_sess_data");
        return -1;
    }

    if (evbuffer_drain(in, (size_t)length) != 0) {
        printf("Error: evbuffer_drain failed");
        return -1;
    }
    return 0;
}

static void readcb(struct bufferevent *bufferEvent, void *ptr){
    cout << "[ read cb ]" << endl;
    auto *clientSessData = (client_sess_data *)ptr;
    (void)bufferEvent;

    int returnValue = session_on_received(clientSessData);
}

static void writecb(struct bufferevent *bufferEvent, void *ptr){
    cout << "[ write cb ]" << endl;
    return;
}


static void accept_callback(struct evconnlistener *conListener, int sock,
                            struct sockaddr *address, int address_length, void *arg) {
    application_ctx *appCtx = (application_ctx *) arg;
    client_sess_data *clientSessData;
    cout << "[ accept_callback]: " << "sock: " << sock << ", address: " << address << endl;
    (void) conListener;

    clientSessData = create_client_session_data(appCtx, sock,address, address_length);
    bufferevent_setcb(clientSessData->bufferEvent, readcb, writecb, event_callback, clientSessData);

}


static void server_listen(struct event_base *eventBase, const char *port, application_ctx *appCtx) {
    /**
     * Sets up and starts the server.
     */

    cout << "[ server_listen ]" << endl;

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
                eventBase, accept_callback, appCtx, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
                16, rp->ai_addr, (int)rp->ai_addrlen);
        if (conListener) {
            freeaddrinfo(res);
            return;
        }
    }

    // if for loop above does not return, starting the listener has failed
    printf("%s", "Error: Could not start listener");
}

static void run(const char *port, const char *certKeyFile, const char *certFile) {
    cout << "[ run ]" << endl;

    SSL_CTX *sslCtx;
    application_ctx appCtx;
    struct event_base *eventBase;

    sslCtx = create_ssl_context();
    configure_context(sslCtx, certKeyFile, certFile);
    eventBase = event_base_new();
    create_application_context(&appCtx, sslCtx, eventBase);

    server_listen(eventBase, port, &appCtx);

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

    init_openssl();

    run(argv[1], argv[2], argv[3]);
    return 0;
}

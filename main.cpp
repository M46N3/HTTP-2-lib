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
#include "h2_config.hpp"
#include "h2_global.hpp"
#include "h2_utils.hpp"

using namespace std;

bool printComments; // Turn off comments.
bool printTrackers; // boolean to turn on/off printing of tracker-comments.
bool printFrames; // boolean to turn on/off printing of frames.
ApplicationContext appCtx;
string publicDir;

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
    //ApplicationContext appCtx;
    struct event_base *eventBase;
    unordered_map<string, string> routes = {};

    sslCtx = h2_config::createSslContext();
    h2_config::configureContext(sslCtx, certKeyFile, certFile);
    eventBase = event_base_new();
    h2_config::createApplicationContext(&appCtx, sslCtx, eventBase, routes);

    // Configure routes and public directory
    h2_utils::setPublicDir("../public");
    h2_utils::addRoute(&appCtx, "/", "/index.html");
    h2_utils::addRoute(&appCtx, "/2", "/index2.html");

    serverListen(eventBase, port, &appCtx);

    event_base_loop(eventBase, 0);

    event_base_free(eventBase);
    SSL_CTX_free(sslCtx);
}

int main(int argc, char **argv) {
    struct sigaction act;

    if (argc < 4) {
        cerr << "http2-server PORT PRIVATE_KEY_FILE CERT_FILE [OPTIONAL FLAG]--verbose\n" << endl;
        exit(EXIT_FAILURE);
    }

    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);

    if (argc == 5 && argv[4] == string("--verbose")) {
        printComments = true; // Turn off comments.
        printTrackers = true; // boolean to turn on/off printing of tracker-comments.
        printFrames = true; // boolean to turn on/off printing of frames.
    } else {
        printComments = false; // Turn off comments.
        printTrackers = false; // boolean to turn on/off printing of tracker-comments.
        printFrames = false; // boolean to turn on/off printing of frames.
    }

    initOpenssl();

    run(argv[1], argv[2], argv[3]);
    return 0;
}
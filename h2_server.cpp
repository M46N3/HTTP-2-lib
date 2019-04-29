// h2_server.cpp

#include "h2_server.hpp"
#include "h2_global.hpp"
#include "h2_structs.hpp"
#include "h2_callbacks.hpp"
#include "h2_config.hpp"
#include "h2_utils.hpp"
#include <iostream>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <event2/listener.h>

using namespace std;

bool printComments; // Turn off comments.
bool printTrackers; // boolean to turn on/off printing of tracker-comments.
bool printFrames; // boolean to turn on/off printing of frames.
ApplicationContext appCtx;
string publicDir;

/// serverListen - Sets up the server and starts listening on the given port.
///
/// @param eventBase - The application-wide event_base object to use with listeners.
/// @param port - The port number the application should listen on.
/// @param appCtx - The global application context object to use.

void h2_server::serverListen(struct event_base *eventBase, const char *port, ApplicationContext *appCtx) {

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

void h2_server::run(const char *port, const char *certKeyFile, const char *certFile) {
    if (printTrackers) {
        cout << "[ run ]" << endl;
    }
    h2_config::initOpenssl();

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

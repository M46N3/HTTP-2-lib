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
#include "src/h2_enums.hpp"
#include <nghttp2/nghttp2.h>
#include <sstream>
#include <fstream>
#include "src/h2_callbacks.hpp"
#include "src/h2_structs.hpp"
#include "src/h2_config.hpp"
#include "src/h2_global.hpp"
#include "src/h2_utils.hpp"
#include "include/HTTP-2-lib/h2_server.hpp"

using namespace std;

int main(int argc, char **argv) {
    struct sigaction act;

    if (argc < 2) {
        cerr << "Nettverksprog_prosjekt PORT [OPTIONAL FLAG]--verbose\n" << endl;
        exit(EXIT_FAILURE);
    }

    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);

    if (argc == 3 && argv[2] == string("--verbose")) {
        printComments = true; // Turn off comments.
        printTrackers = true; // boolean to turn on/off printing of tracker-comments.
        printFrames = true; // boolean to turn on/off printing of frames.
    } else {
        printComments = false;
        printTrackers = false;
        printFrames = false;
    }

    // Initialize server instance with Cert and private key.
    h2_server server = h2_server("../key.pem", "../cert.pem");

    // Set public dir and add routes.
    h2_server::setPublicDir("../public");
    h2_server::addRoute("/", "/index.html");
    h2_server::addRoute("/2", "/index2.html");

    // Start the server.
    server.run(argv[1]);
    return 0;
}
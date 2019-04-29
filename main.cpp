#include <stdio.h>
#include <string.h>
#include <openssl/ssl.h>
#include <iostream>
#include <signal.h>
#include "src/h2_global.hpp"
#include "include/HTTP-2-lib/h2_server.hpp"

using namespace std;

int main(int argc, char **argv) {
    struct sigaction act;

    if (argc < 2) {
        cerr << "HTTP-2-lib-server PORT [OPTIONAL FLAG]--verbose\n" << endl;
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
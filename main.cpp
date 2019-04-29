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
#include "h2_server.hpp"

using namespace std;

/*
bool printComments; // Turn off comments.
bool printTrackers; // boolean to turn on/off printing of tracker-comments.
bool printFrames; // boolean to turn on/off printing of frames.
ApplicationContext appCtx;
string publicDir;
*/

void cleanup_openssl() {
    EVP_cleanup();
}

int main(int argc, char **argv) {
    struct sigaction act;

    if (argc < 4) {
        cerr << "Nettverksprog_prosjekt PORT PRIVATE_KEY_FILE CERT_FILE [OPTIONAL FLAG]--verbose\n" << endl;
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

    h2_server::run(argv[1], argv[2], argv[3]);
    return 0;
}
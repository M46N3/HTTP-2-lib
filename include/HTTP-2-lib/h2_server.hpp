// h2_server.hpp

#include "../../src/h2_structs.hpp"

class h2_server {
public:
    static void serverListen(struct event_base *eventBase, const char *port, ApplicationContext *appCtx);
    static void run(const char *port, const char *certKeyFile, const char *certFile);
};
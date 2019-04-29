// h2_server.hpp

#include "../../src/h2_structs.hpp"

class h2_server {
public:
    h2_server(const char *certKeyFile, const char *certFile);
    static void setPublicDir(string dir);
    static void addRoute(string path, string filepath);
    void run(const char *port);

private:
    static void serverListen(struct event_base *eventBase, const char *port, ApplicationContext *appCtx);
    struct event_base *eventBase;
    SSL_CTX *sslCtx;
};
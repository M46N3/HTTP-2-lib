// h2_callbacks.hpp

#pragma once

class h2_callbacks {
public:
    static void readCallback(struct bufferevent *bufferEvent, void *ptr);
    static void writeCallback(struct bufferevent *bufferEvent, void *ptr);
    static void eventCallback(struct bufferevent *bufferEvent, short events, void *ptr);
    static void acceptCallback(struct evconnlistener *conListener, int sock,
                               struct sockaddr *address, int address_length, void *arg);
};



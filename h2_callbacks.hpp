// h2_callbacks.hpp
#ifndef NETTVERKSPROG_PROSJEKT_H2_CALLBACKS_HPP
#define NETTVERKSPROG_PROSJEKT_H2_CALLBACKS_HPP

class h2_callbacks {
public:
    static void readCallback(struct bufferevent *bufferEvent, void *ptr);
    static void eventCallback(struct bufferevent *bufferEvent, short events, void *ptr);
};

#endif //NETTVERKSPROG_PROSJEKT_H2_CALLBACKS_HPP

// h2_callbacks.cpp

#include "h2_structs.hpp"
#include "h2_callbacks.hpp"
#include "h2_utils.hpp"
#include "h2_global.hpp"
#include <iostream>
#include <event2/bufferevent_ssl.h>

using namespace std;


/// Callback triggered when there is data to be read in the evbuffer.
///
/// @param bufferEvent - The bufferevent that triggered the callback.
/// @param ptr - The user-specified context for this bufferevent, which is the ClientSessionData object.

void h2_callbacks::readCallback(struct bufferevent *bufferEvent, void *ptr) {
    if (printTrackers) cout << "[ readCallback ]" << endl;
    auto *clientSessData = (ClientSessionData *)ptr;
    (void)bufferEvent;

    int returnValue = h2_utils::sessionOnReceived(clientSessData);
}


/// Callback invoked when all data in the session's buffer has been sent and the buffer is empty.
/// It has not been implemented as of yet, but with time the library will change from responding to client requests
/// directly with a bufferwrite, to adding response data to a linked list queued for transmission and sending it by
/// invoking a function 'sendSessionData()'. With time this will be implemented.
///
/// \param bufferEvent - The connections bufferevent object.
/// \param ptr - The connections ClientSessionData object, which will hold the linked list of data
///              queued for transmission

void h2_callbacks::writeCallback(struct bufferevent *bufferEvent, void *ptr){
    if (printTrackers) {
        cout << "[ write cb ]" << endl;
    }
}


/// Callback invoked when there is an event on the sock filedescriptor.
///
/// @param bufferEvent - bufferevent object associated with the connection.
/// @param events - flag type conjunction of error and/or event type.
/// @param ptr - clientSessionData object for the connection.

void h2_callbacks::eventCallback(struct bufferevent *bufferEvent, short events, void *ptr) {
    if (printTrackers) {
        cout << " [ eventCallback ] " << endl;
    }

    auto *clientSessData = (ClientSessionData *)ptr;


    if (events & BEV_EVENT_CONNECTED) {
        const unsigned char *alpn = nullptr;
        unsigned int alpnlen = 0;

        SSL *ssl;
        (void)bufferEvent;

        printf( "%s connected\n", clientSessData->clientAddress);

        ssl = bufferevent_openssl_get_ssl(bufferEvent);

        /* Negotiate ALPN on initial connection */
        SSL_get0_alpn_selected(ssl, &alpn, &alpnlen);


        if (alpn == nullptr || alpnlen != 2 || memcmp("h2", alpn, 2) != 0) {
            printf("%s h2 negotiation failed\n", clientSessData->clientAddress);
            h2_utils::deleteClientSessionData(clientSessData);
            return;
        }

        if(h2_utils::sendConnectionHeader(clientSessData) != 0) {
            h2_utils::deleteClientSessionData(clientSessData);
            return;
        }
        return;
    }
    h2_utils::deleteClientSessionData(clientSessData);
}


/// Creates a ClientSessionData object for the new accepted connection and sets
/// callbacks for the created bufferevent object.
///
/// \param conListener - evconnlistener struct object
/// \param sock - filedscriptor for the connection
/// \param address - clients address
/// \param address_length - length of clients address
/// \param arg  - user supplied ApplicationContext object.

void h2_callbacks::acceptCallback(struct evconnlistener *conListener, int sock,
                                  struct sockaddr *address, int address_length, void *arg) {
    auto *appCtx = (ApplicationContext *) arg;
    ClientSessionData *clientSessData;

    if (printTrackers) {
        cout << "[ acceptCallback]: " << "sock: " << sock << ", address: " << address << endl;
    }

    (void) conListener;

    clientSessData = h2_utils::createClientSessionData(appCtx, sock, address, address_length);
    bufferevent_setcb(clientSessData->bufferEvent, readCallback, writeCallback, eventCallback, clientSessData);
}
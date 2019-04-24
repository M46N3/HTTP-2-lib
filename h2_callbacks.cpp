// h2_callbacks.cpp
#include "h2_callbacks.hpp"
#include <iostream>
#include <string.h>
#include <event2/bufferevent_ssl.h>
#include <openssl/ssl.h>

using namespace std;

/// readCallback - Callback triggered when there is data to be read in the evbuffer.
///
/// @param bufferEvent - The bufferevent that triggered the callback.
/// @param ptr - The user-specified context for this bufferevent, which is the ClientSessionData object.
void h2_callbacks::readCallback(struct bufferevent *bufferEvent, void *ptr) {
    cout << "[ readCallback ]" << endl;
    auto *clientSessData = (client_sess_data *)ptr;
    (void)bufferEvent;

    int returnValue = sessionOnReceived(clientSessData);
}

void h2_callbacks::eventCallback(struct bufferevent *bufferEvent, short events, void *ptr) {
    client_sess_data *clientSessData = (client_sess_data *)ptr;


    if (events & BEV_EVENT_CONNECTED) {
        const unsigned char *alpn = NULL;
        unsigned int alpnlen = 0;

        SSL *ssl;
        (void)bufferEvent;

        printf( "%s connected\n", clientSessData->clientAddress);

        ssl = bufferevent_openssl_get_ssl(bufferEvent);

        /* Negotiate ALPN on initial connection */
        if (alpn == NULL) {
            SSL_get0_alpn_selected(ssl, &alpn, &alpnlen);
        }

        if (alpn == NULL || alpnlen != 2 || memcmp("h2", alpn, 2) != 0) {
            printf("%s h2 negotiation failed\n", clientSessData->clientAddress);
            /* TODO: delete_client_sess_data(clientSessData); */
            return;
        }

        /*
        if(send_connection_header(clientSessData) != 0 ||
            client_sess_send(clientSessData) != 0) {
            // TODO: delete_client_sess_data(clientSessData);
            return;
        }

        return;
        */
    }
}
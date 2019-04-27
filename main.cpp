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
#include "frames.hpp"
#include <nghttp2/nghttp2.h>
#include <sstream>
#include <fstream>

using namespace std;

static unsigned char next_proto_list[256];
static size_t next_proto_list_len;
static bool printComments = true; // Turn off comments.
static bool printTrackers = true; // boolean to turn on/off printing of tracker-comments.
static bool printFrames = true; // boolean to turn on/off printing of frames.


void initOpenssl() {

    if (printTrackers) {
        cout << "[ initOpenssl ]" << endl;
    }

    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}


/// createSslContext - Creates a new SSL_CTX object and sets the connection method to be used,
/// which is a general SSL/TLS server connection method.
///
/// return ctx - The newly created SSL_CTX object with the connection method set.

SSL_CTX *createSslContext() {

    if (printTrackers) {
        cout << "[ createSslContext ]" << endl;
    }

    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}


/// nextProtocolCallback -
///
/// \param s
/// \param data
/// \param len
/// \param arg
/// \return

static int nextProtocolCallback(SSL *s, const unsigned char **data,
                                unsigned int *len, void *arg) {

    if (printTrackers) {
        cout << "[ nextProtocolCallback ]" << endl;
    }

    *data = next_proto_list;
    *len = (unsigned int)next_proto_list_len;
    return SSL_TLSEXT_ERR_OK;
}


/// selectProtocol - Selects 'h2' meaning 'HTTP/2 over TLS'
///
/// @param out - name of protocol chosen by the server.
/// @param outlen - length of name of protocol given in 'out'.
/// @param in  - string of protocols supported by the client, prefixed by the length of the following protocol.
/// @param inlen - total length of the protocols-string 'in'.
/// @return - 1 if successful.

static int selectProtocol(unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen) {

    if (printTrackers) {
        cout << "[ selectProtocol ]" << endl;
    }

    unsigned int start_index = (unsigned int) in[0];
    unsigned int end_of_next_protocol = start_index ;

    unsigned int i = 0;

    if (printComments) {
        cout << endl << "Client supports following protocols, ordered according to preference: " << endl;
        while (i < inlen) {
            while (i <= end_of_next_protocol && i < inlen) {
                cout << in[i];
                ++i;
            }
            cout << endl;
            end_of_next_protocol += in[i] + 1;
            ++i;

        }
        cout << endl;
    }


    *outlen = in[0];
    out[0] = (unsigned char*) "h2";

    return 1;
}


/// alpnSelectProtocolCallback - Callback function used for the negotiation of HTTP/2 over TLS in ALPN.
///
/// @param ssl - SSL object (unused parameter).
/// @param out - string for chosen protocol
/// @param outlength - length of name of chosen protocol given in 'out'.
/// @param in - protocols supported by the client.
/// @param inlen - length of 'in'.
/// @param arg
/// @return - returns 0 on success and non-zero on failure.

static int alpnSelectProtocolCallback(SSL *ssl, const unsigned char **out,
                                      unsigned char *outlength, const unsigned char *in,
                                      unsigned int inlen, void *arg) {

    if (printTrackers) {
        cout << "[ alpnSelectProtocolCallback ]" << endl;
    }

    int rv;

    rv = selectProtocol((unsigned char **) out, outlength, in, inlen);

    if (rv != 1) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    if (printComments) {
        cout << "Server has chosen 'h2', meaning HTTP/2 over TLS" << endl;
    }

    return SSL_TLSEXT_ERR_OK;
}


/// configureAlpn - Configures the referenced SSL_CTX object to use the Aplication Protocol Layer Negotiation
/// extension, with 'nextProtocolCallback' and 'alpnSelectProtocolCallback'.
///
/// @param ctx - reference to the SSL_CTX object to configure ALPN extension on.

void configureAlpn(SSL_CTX *ctx) {
    if (printTrackers) {
        cout << "[ configureAlpn ]" << endl;
    }

    next_proto_list[0] = 2;
    memcpy(&next_proto_list[1], "h2", 2);
    next_proto_list_len = 1 + 2;

    SSL_CTX_set_next_protos_advertised_cb(ctx, nextProtocolCallback, NULL);

    SSL_CTX_set_alpn_select_cb(ctx, alpnSelectProtocolCallback, NULL);
}


/// configureContext - Configures the SSL_CTX object to use given certificate and private key,
/// and calls to configure ALPN extension.
///
/// @param ctx - reference to the SSL_CTX object to configure.
/// @param certKeyFile - path to Certificate Key File to be used for TLS.
/// @param certFile - path to Certificate File to be used for TLS.

void configureContext(SSL_CTX *ctx, const char *certKeyFile, const char *certFile) {
    if (printTrackers) {
        cout << "[ configureContext ]" << endl;
    }

    /* Make server always choose the most appropriate curve for the client. */
    SSL_CTX_set_ecdh_auto(ctx, 1);

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, certFile, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, certKeyFile, SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* Configure SSL_CTX object to use ALPN */
    configureAlpn(ctx);
}

struct application_ctx {
    SSL_CTX *ctx;
    struct event_base *eventBase;
};

struct stream_data {
    struct stream_data *prev, *next;
    char *requested_path;
    int32_t stream_id;
    int sock;
} stream_data;

/* client_sess_data structure to store data pertaining to one h2 connection */
struct client_sess_data {
    struct stream_data root;
    struct bufferevent *bufferEvent;
    application_ctx *appCtx;
    //h2_session *session;
    char *clientAddress;
    nghttp2_hd_inflater *inflater;
    nghttp2_hd_deflater *deflater;
} client_session_data;


/// createClientSessionData - Creates a clientSessionData object which keeps all to be used for one single connection.
///
/// @param appCtx - Application-wide application_ctx object.
/// @param sock - file descriptor for the connection
/// @param clientAddress - socket address of the client.
/// @param addressLength - length of socket address.
/// @return clientSessData - the newly created

static client_sess_data *createClientSessionData(application_ctx *appCtx, int sock, struct sockaddr *clientAddress,
                                                 int addressLength) {
    int returnValue;
    client_sess_data *clientSessData;
    SSL *ssl;
    char host[1025];
    int val = 1;

    /* Create new TLS session object */
    ssl = SSL_new(appCtx->ctx);
    if (!ssl) {
        errx(1, "Could not create TLS session object: %s",
                ERR_error_string(ERR_get_error(), NULL));
    }

    clientSessData = (client_sess_data *)malloc(sizeof(client_sess_data));
    memset(clientSessData, 0, sizeof(client_sess_data));

    clientSessData->appCtx = appCtx;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val)); // set TCP_NODELAY option for improved latency

    clientSessData->bufferEvent = bufferevent_openssl_socket_new(
            appCtx->eventBase, sock, ssl, BUFFEREVENT_SSL_ACCEPTING,
            BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS
            );

    bufferevent_enable(clientSessData->bufferEvent, EV_READ | EV_WRITE);

    returnValue = getnameinfo(clientAddress, (socklen_t)addressLength, host, sizeof(host), NULL, 0, NI_NUMERICHOST);

    if (returnValue != 0) {
        clientSessData->clientAddress = strdup("(unknown)");
    } else {
        clientSessData->clientAddress = strdup(host);
    }

    // Initiate HPACK inflater
    int rv = nghttp2_hd_inflate_new(&clientSessData->inflater);

    if (rv != 0) {
        fprintf(stderr, "nghttp2_hd_inflate_init failed with error: %s\n",
                nghttp2_strerror(rv));
        exit(EXIT_FAILURE);
    }

    // Initiate HPACK deflater
    rv = nghttp2_hd_deflate_new(&clientSessData->deflater, 4096);

    if (rv != 0) {
        fprintf(stderr, "nghttp2_hd_deflate_init failed with error: %s\n",
                nghttp2_strerror(rv));
        exit(EXIT_FAILURE);
    }

    return clientSessData;

}


/// createApplicationContext - Initializes the application wide application_ctx object on the refernece given.
///
/// @param appCtx - reference to appCtx object to initialize.
/// @param sslCtx - SSL_CTX object to use.
/// @param eventBase_ - event_base object to use.

static void createApplicationContext(application_ctx *appCtx, SSL_CTX *sslCtx, struct event_base *eventBase_) {
    /**
     * Sets the application_ctx members, ctx and eventBase, to the given SSL_CTX and event_base objects
     */

    if (printTrackers) {
        cout << "[ createApplicationContext ]" << endl;
    }

    memset(appCtx, 0, sizeof(application_ctx));
    appCtx->ctx = sslCtx;
    appCtx->eventBase = eventBase_;
}


/// sendConnectionHeader - Sends server connection header with 'MAXIMUM_CONCURRENT_STREAMS' set to 100.
///
/// @param ClientSessData - ClientSessData object for this particular connection.
/// @return returnValue - 0 if successful, non-zero if failure.

static int sendConnectionHeader(client_sess_data *ClientSessData) {
    int returnValue;
    if (printComments) {
        cout << "\nSending server SETTINGS FRAME." << endl;
    }

    char data[] = { 0x00, 0x00, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x64 };

    returnValue = bufferevent_write(ClientSessData->bufferEvent, data, 15);
    return returnValue;
}

#define MAKE_NV(K, V)                                                          \
  {                                                                            \
    (uint8_t *)K, (uint8_t *)V, sizeof(K) - 1, sizeof(V) - 1,                  \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }


static void sendGetResponse(client_sess_data *ClientSessData, const unsigned char *data) {
    // HEADER FRAME:
    ssize_t rv;

    nghttp2_nv nva[] = {
            MAKE_NV(":status", "200"),
            MAKE_NV("content-type", "text/html;charset=UTF-8")
    };

    size_t nvlen = sizeof(nva) / sizeof(nva[0]);

    uint8_t *buf;
    size_t buflen;
    size_t outlen;
    size_t i;
    size_t sum = 0;

    for (i = 0; i < nvlen; ++i) {
        sum += nva[i].namelen + nva[i].valuelen;
    }

    //printf("Input (%zu byte(s)):\n\n", sum);
    cout << "\n\nHEADER FIELDS SENT:" << endl;

    for (i = 0; i < nvlen; ++i) {
        fwrite(nva[i].name, 1, nva[i].namelen, stdout);
        printf(": ");
        fwrite(nva[i].value, 1, nva[i].valuelen, stdout);
        printf("\n");
    }
    printf("\n");

    buflen = nghttp2_hd_deflate_bound(ClientSessData->deflater, nva, nvlen);
    buf = static_cast<uint8_t *>(malloc(buflen));

    rv = nghttp2_hd_deflate_hd(ClientSessData->deflater, buf, buflen, nva, nvlen);

    if (rv < 0) {
        fprintf(stderr, "nghttp2_hd_deflate_hd() failed with error: %s\n",
                nghttp2_strerror((int)rv));

        free(buf);

        exit(EXIT_FAILURE);
    }

    outlen = (size_t)rv;
    //cout << "outlen: " << outlen << endl;

    unsigned char frameHeader[] = { 0x00, 0x00, (unsigned char) outlen,     // Length
                                    Types::HEADERS,                         // Type
                                    END_HEADERS,                            // Flags
                                    data[5], data[6], data[7], data[8]};               // Stream-ID
    auto *frame = new unsigned char[9 + outlen];

    for (size_t i = 0; i < 9; ++i) frame[i] = frameHeader[i];
    for (size_t i = 0; i < outlen; ++i) frame[i+9] = buf[i];

    bufferevent_write(ClientSessData->bufferEvent, frame, outlen+9);

    // DATA FRAME:
    string s;
    ifstream in("../index.html");
    if (in) {
        s = string((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    } else {
        cout << "Could not read index.html" << endl;
        // TODO: Return 404, check if file is found before sending 200
    }
    auto sLen = s.length();
    //cout << "sLen: " << sLen << endl;

    size_t sLen1 = sLen;
    size_t sLen2 = 0;
    size_t sLen3 = 0;
    if (sLen > 255 && sLen < 65536) {                   // Big enough to need 2 bytes to represent length
        sLen2 = sLen / 256;
        sLen1 = sLen % 256;
        // cout << sLen2 << "\t" << sLen1 << endl;
    } else if (sLen > 65535 && sLen < 16777216) {       // Big enough to need 3 bytes to represent length
        sLen3 = sLen / 65536;
        sLen2 = (sLen % 65536) / 256;
        sLen1 = sLen % 256;
        // cout << sLen3 << "\t" <<sLen2 << "\t" << sLen1 << endl;
    } else {                                            // Big enough to need more than 1 frame
        // TODO: Handle payloads, long enough to need more than 1 frame
    }

    unsigned char frameHeader2[] = {(unsigned char) sLen3, (unsigned char) sLen2, (unsigned char) sLen1,    // Length
                                    DATA,                                                                   // Type
                                    END_STREAM,                                                             // Flags
                                    data[5], data[6], data[7], data[8]};                                                // Stream-ID
    auto *frame2 = new unsigned char[9 + sLen];

    for (size_t i = 0; i < 9; ++i) frame2[i] = frameHeader2[i];
    for (size_t i = 0; i < sLen; ++i) frame2[i+9] = s[i];

    bufferevent_write(ClientSessData->bufferEvent, frame2, sLen+9);
}

/// eventCallback - Callback invoked when there is an event on the sock filedescriptor.
///
/// @param bufferEvent - bufferevent object associated with the connection.
/// @param events - flag type conjunction of error and/or event type.
/// @param ptr - clientSessionData object for the connection.

static void eventCallback(struct bufferevent *bufferEvent, short events, void *ptr){
    if (printTrackers) {
        cout << " [ eventCallback ] " << endl;
    }

    client_sess_data *clientSessData = (client_sess_data *)ptr;


    if (events & BEV_EVENT_CONNECTED) {
        const unsigned char *alpn = NULL;
        unsigned int alpnlen = 0;

        SSL *ssl;
        (void)bufferEvent;

        if (printComments) {
            printf( "%s connected\n", clientSessData->clientAddress);
        }

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

        if(sendConnectionHeader(clientSessData) != 0) {
            // TODO: delete_client_sess_data(clientSessData);
            return;
        }
        return;
    }
}

static string bytesToString(const unsigned char *data, size_t firstIndex, size_t secondIndex) {
    static const char characters[] = "0123456789abcdef";

    std::string res((secondIndex - firstIndex) * 2, 0);

    char *buf = (res.data());

    for (size_t i = firstIndex; i < secondIndex; ++i) {
        *buf++ = characters[data[i] >> 4];
        *buf++ = characters[data[i] & 0x0F];
    }

    return res;
}


static ulong hexToUlong(string hexString) {
    ulong hexDecimalValue;
    if (hexString[0] == '0' && hexString[1] == 'x') {
        std::istringstream iss(hexString);
        iss >> std::hex >> hexDecimalValue;
    } else {
        string hex = "0x" + hexString;
        std::istringstream iss(hex);
        iss >> std::hex >> hexDecimalValue;
    }
    return hexDecimalValue;
}

static void dataFrameHandler(const unsigned char *data) {
    // Handle payload
}

static void headerFrameHandler(client_sess_data *clientSessData, const unsigned char *data, size_t length) {
    // Checks if the padded and priority flags for the header frame are set
    const bool padded = bitset<8>(data[4])[3];      // PADDED = 0x8
    const bool priority = bitset<8>(data[4])[5];    // PRIORITY = 0x20

    cout << "\nPayload:";

    ssize_t rv;

    ulong padlength = 0;
    if (padded) {
        padlength = bitset<8>(data[9]).to_ulong();
    }

    auto *in = (uint8_t*)data + 9 + (priority ? 5 : 0) + (padded ? 1 : 0);
    size_t inlen = length - 9 - (priority ? 5 : 0) - padlength;

    for (;;) {
        nghttp2_nv nv;
        int inflate_flags = 0;
        size_t proclen;
        int in_final = 1 ;

        rv = nghttp2_hd_inflate_hd(clientSessData->inflater, &nv, &inflate_flags, in, inlen, in_final);

        if (rv < 0) {
            fprintf(stderr, "inflate failed with error code %zd", rv);
            exit(EXIT_FAILURE);
        }

        proclen = (size_t)rv;

        in += proclen;
        inlen -= proclen;

        if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
            printf("\n%s : %s", nv.name, nv.value);
        }

        if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
            nghttp2_hd_inflate_end_headers(clientSessData->inflater);
            break;
        }

        if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && inlen == 0) {
            break;
        }
    }
    sendGetResponse(clientSessData, data);
}

static void settingsFrameHandler(client_sess_data *clientSessData, const unsigned char *data, size_t length) {
    // Printing SETTINGS frame
    if (printFrames) {
        size_t indexIdentifier = 9;
        size_t indexValue = 11;
        size_t valuePrint = 14;
        int payloadNumber = 1;
        for (size_t i = 9; i < length; ++i) {
            if (i == indexIdentifier) {
                payloadNumber++;
                cout << "\nIdentifier(16):\t\t\t\t";
                indexIdentifier += 6;
            }
            if (i == indexValue) {
                switch (data[i-1]) {
                    case SettingsParameters::SETTINGS_HEADER_TABLE_SIZE:
                        cout << "\t\tSETTINGS_HEADER_TABLE_SIZE";
                        break;
                    case SettingsParameters::SETTINGS_ENABLE_PUSH:
                        cout << "\t\tSETTINGS_ENABLE_PUSH";
                        break;
                    case SettingsParameters::SETTINGS_MAX_CONCURRENT_STREAMS:
                        cout << "\t\tSETTINGS_MAX_CONCURRENT_STREAMS";
                        break;
                    case SettingsParameters::SETTINGS_INITIAL_WINDOW_SIZE:
                        cout << "\t\tSETTINGS_INITIAL_WINDOW_SIZE";
                        break;
                    case SettingsParameters::SETTINGS_MAX_FRAME_SIZE:
                        cout << "\t\tSETTINGS_MAX_FRAME_SIZE";
                        break;
                    case SettingsParameters::SETTINGS_MAX_HEADER_LIST_SIZE:
                        cout << "\t\tSETTINGS_MAX_HEADER_LIST_SIZE";
                        break;
                    default:
                        break;
                }
                cout << "\nValue(32):\t\t\t\t\t";
                indexValue += 6;
            }
            printf("%02x", data[i]);
            if (i == valuePrint) {
                cout << "\t" << hexToUlong(bytesToString(data, (valuePrint-3), (valuePrint+1)));
                valuePrint += 6;
            }
        }
        cout << endl << endl;
    }


//    TODO: Kan kommentarene her fjernes?
//    const bool padded = bitset<8>(data[4])[3];
//    const bool priority = bitset<8>(data[4])[5];
//
//    ulong streamID = hexToUlong(bytesToString(data, 5, 9));
//    cout << "StreamID: " << streamID << endl;
//
//    if (flagArray[7] == '1' && payloadLength != 0) {
//        cout << "yippie kay yay madafaka" << endl;
//
//        // FRAME_SIZE_ERROR (0x6):
//        char dataSend[] = { 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00 };
//        bufferevent_write(clientSessData->bufferEvent, dataSend, 9);
//    }


//    if () {
//
//    }


//    ulong payloadLength = bitset<24>(data[0] + data[1] + data[2]).to_ulong();
//    cout << endl;
//    cout << "Payload length: " << payloadLength << endl;
//
//    string flagString = bitset<8>(data[4]).to_string();
//
//    char flagArray[8] = {0};
//    std::copy(flagString.begin(), flagString.end(), flagArray);
    // ACK
//            char data[] = { 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00 };
//            bufferevent_write(clientSessData->bufferEvent, data, 9);

//    cout << "\nFlag ACK: " << flagArray[7] << endl;
//    cout << "payloadLength: " << payloadLength << endl;
//    if (payloadLength % 6 != 0) {
//        char data[] = { 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00 };
//        bufferevent_write(clientSessData->bufferEvent, data, 9);
//    }
}

static void pingFrameHandler(client_sess_data *clientSessData, const unsigned char *data, size_t length) {
    const bool ack = bitset<8>(data[4])[0];      // ACK = 0x1

    // Respond to ping if ack flag is not set
    if (!ack) {
        auto *response = new unsigned char[length];
        for (size_t i = 0; i < length; ++i) response[i] = data[i];
        response[4] = 0x1;
        bufferevent_write(clientSessData->bufferEvent, response, length);
        cout << "\nResponded to PING frame" << endl;
    } else {
        cout << "\nRevieved PING response" << endl;
    }
}

static void windowUpdateFrameHandler(client_sess_data *clientSessData, const unsigned char *data, size_t length) {
    // Printing WINDOW_UPDATE frame
    if (printFrames) {
        size_t windowSizeIncrementValueIndex = 9;
        size_t valuePrint = 12;
        for (size_t i = 9; i < length; ++i) {
            if (i == windowSizeIncrementValueIndex) {
                cout << "\nWindow Size Increment(31):\t";
                windowSizeIncrementValueIndex += 4;
            }
            printf("%02x", data[i]);
            if (i == valuePrint) {
                cout << "\t" << hexToUlong(bytesToString(data, (valuePrint-3), (valuePrint+1))) << " octets??";
                valuePrint += 4;
            }
        }
        cout << endl << endl;
    }
}

static void frameDefaultPrint(const unsigned char *data) {
    // Printing Frame Format
    if (printFrames) {
        for (size_t i = 0; i < 9; ++i) {
            if (i == 0) cout << "Length(24):\t\t\t\t\t";
            if (i == 3) {
                cout << "\t\t" << hexToUlong(bytesToString(data, (0), (3))) << " octets";
                cout << "\nType(8):\t\t\t\t\t";
            }
            if (i == 4) {
                cout << "\t\t\t";
                switch (data[3]) {
                    case Types::DATA:
                        cout << "DATA";
                        break;
                    case Types::HEADERS:
                        cout << "HEADER";
                        break;
                    case Types::PRIORITY:
                        cout << "PRIORITY";
                        break;
                    case Types::RST_STREAM:
                        cout << "RST_STREAM";
                        break;
                    case Types::SETTINGS:
                        cout << "SETTINGS";
                        break;
                    case Types::PUSH_PROMISE:
                        cout << "PUSH_PROMISE";
                        break;
                    case Types::PING:
                        cout << "PING";
                        break;
                    case Types::GOAWAY:
                        cout << "GOAWAY";
                        break;
                    case Types::WINDOW_UPDATE:
                        cout << "WINDOW_UPDATE";
                        break;
                    case Types::CONTINUATION:
                        cout << "CONTINUATION";
                        break;
                    default:
                        cout << "UNKNOWN";
                        break;
                }
                cout << "\nFlags(8)(bits):\t\t\t\t";
            }
            if (i == 5) cout << "\nStream Identifier(R + 31):\t";
            if (i == 4) {
                cout << bitset<8>(data[i]);
            } else {
                printf("%02x", data[i]);
            }
            if (i == 8) cout << "\t" << hexToUlong(bytesToString(data, 5, 9));
        }
    }
}

static void frameHandler(client_sess_data *clientSessData, const unsigned char *data, size_t length) {
    switch (data[3]) {
        case Types::DATA:
            frameDefaultPrint(data);
            break;
        case Types::HEADERS:
            frameDefaultPrint(data);
            headerFrameHandler(clientSessData, data, length);
            break;
        case Types::PRIORITY:
            frameDefaultPrint(data);
            break;
        case Types::RST_STREAM:
            frameDefaultPrint(data);
            break;
        case Types::SETTINGS:
            frameDefaultPrint(data);
            settingsFrameHandler(clientSessData, data, length);
            break;
        case Types::PUSH_PROMISE:
            frameDefaultPrint(data);
            break;
        case Types::PING:
            frameDefaultPrint(data);
            pingFrameHandler(clientSessData, data, length);
            break;
        case Types::GOAWAY:
            frameDefaultPrint(data);
            break;
        case Types::WINDOW_UPDATE:
            frameDefaultPrint(data);
            windowUpdateFrameHandler(clientSessData, data, length);
            break;
        case Types::CONTINUATION:
            frameDefaultPrint(data);
            break;
        default:
            string connectionPreface = "505249202a20485454502f322e300d0a0d0a534d0d0a0d0a";
            string dataString = bytesToString(data, 0, 24);

            if (connectionPreface == dataString) {
                if (printFrames) {
                    cout << "Connection preface:" << endl;

                    for(size_t i = 0; i < 24; ++i) {
                        cout << data[i];
                    }

                    for(size_t i = 0; i < 24; ++i) {
                        printf("%02x", data[i]);
                    }

                    cout << endl << endl;

                    if (length > 24) {
                        for(size_t i = 24; i < length; ++i) {
                            printf("%02x", data[i]);
                        }

                        cout << endl << endl;

                        ulong currentPos = 24;
                        while (length > currentPos) {
                            ulong nextFrameLength = hexToUlong(bytesToString(data, currentPos, (currentPos + 3)));
                            ulong nextFrameTotLength = nextFrameLength + 9;
                            frameHandler(clientSessData, (data+currentPos), nextFrameTotLength);
                            currentPos += nextFrameTotLength;
                        }
                    }
                }
            } else {
                if (printFrames) {
                    cout << "Frame type is unknown" << endl;
                }
            }
            break;
    }
}

static int sessionOnReceived(client_sess_data *clientSessData) {

    /* TODO: Remove readlen, or make it function */
    ssize_t readlen;
    struct evbuffer *in = bufferevent_get_input(clientSessData->bufferEvent);
    size_t length = evbuffer_get_length(in);
    unsigned char *data = evbuffer_pullup(in, -1); // Make whole buffer contiguous

    frameHandler(clientSessData, data, length);

    readlen = 1;
    if (readlen < 0) {
        printf("Error: error storing recevied data in client_sess_data");
        return -1;
    }
    if (evbuffer_drain(in, (size_t)length) != 0) {
        printf("Error: evbuffer_drain failed");
        return -1;
    }
    return 0;
}


/// readCallback - Callback triggered when there is data to be read in the evbuffer.
///
/// @param bufferEvent - The bufferevent that triggered the callback.
/// @param ptr - The user-specified context for this bufferevent, which is the ClientSessionData object.

static void readCallback(struct bufferevent *bufferEvent, void *ptr){
    if (printTrackers) {
        cout << "[ readCallback ]" << endl;
    }
    auto *clientSessData = (client_sess_data *)ptr;
    (void)bufferEvent;

    int returnValue = sessionOnReceived(clientSessData);
}


/// writeCallback - Callback that gets invoked when all data in the bufferevent output buffer has been sent.
///
/// @param bufferEvent -
/// @param ptr

static void writeCallback(struct bufferevent *bufferEvent, void *ptr){
    if (printTrackers) {
        cout << "[ write cb ]" << endl;
    }
    return;
}


static void acceptCallback(struct evconnlistener *conListener, int sock,
                           struct sockaddr *address, int address_length, void *arg) {
    application_ctx *appCtx = (application_ctx *) arg;
    client_sess_data *clientSessData;

    if (printTrackers) {
        cout << "[ acceptCallback]: " << "sock: " << sock << ", address: " << address << endl;
    }


    (void) conListener;

    clientSessData = createClientSessionData(appCtx, sock, address, address_length);
    bufferevent_setcb(clientSessData->bufferEvent, readCallback, writeCallback, eventCallback, clientSessData);

}


/// serverListen - Sets up the server and starts listening on the given port.
///
/// @param eventBase - The application-wide event_base object to use with listeners.
/// @param port - The port number the application should listen on.
/// @param appCtx - The global application context object to use.

static void serverListen(struct event_base *eventBase, const char *port, application_ctx *appCtx) {

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
                eventBase, acceptCallback, appCtx, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
                16, rp->ai_addr, (int) rp->ai_addrlen);
        if (conListener) {
            freeaddrinfo(res);
            return;
        }
    }

    // if for loop above does not return, starting the listener has failed
    printf("%s", "Error: Could not start listener");
}

static void run(const char *port, const char *certKeyFile, const char *certFile) {
    if (printTrackers) {
        cout << "[ run ]" << endl;
    }

    SSL_CTX *sslCtx;
    application_ctx appCtx;
    struct event_base *eventBase;

    sslCtx = createSslContext();
    configureContext(sslCtx, certKeyFile, certFile);
    eventBase = event_base_new();
    createApplicationContext(&appCtx, sslCtx, eventBase);

    serverListen(eventBase, port, &appCtx);

    event_base_loop(eventBase, 0);

    event_base_free(eventBase);
    SSL_CTX_free(sslCtx);
}

int main(int argc, char **argv) {
    struct sigaction act;

    if (argc < 4) {
        cerr << "http2-server PORT PRIVATE_KEY_FILE CERT_FILE\n" << endl;
        exit(EXIT_FAILURE);
    }

    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, NULL);

    initOpenssl();

    run(argv[1], argv[2], argv[3]);
    return 0;
}
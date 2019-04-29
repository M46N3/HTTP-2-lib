// h2_utils.cpp

#include "h2_utils.hpp"
#include "h2_structs.hpp"
#include "h2_frame_handlers.hpp"
#include "h2_enums.hpp"
#include "h2_global.hpp"
#include <iostream>
#include <string.h>
#include <nghttp2/nghttp2.h>
#include <event.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent_ssl.h>
#include <openssl/err.h>
#include <err.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sstream>
#include <fstream>
#include <regex>
#include <iterator>

using namespace std;


/// Sends server connection header with 'MAXIMUM_CONCURRENT_STREAMS' set to 100.
///
/// @param ClientSessData - ClientSessionData object for this particular connection.
/// @return returnValue - 0 if successful, non-zero if failure.

int h2_utils::sendConnectionHeader(struct ClientSessionData *clientSessData) {
    int returnValue;
    if (printComments) {
        cout << "\nSending server SETTINGS FRAME." << endl;
    }

    char data[] = { 0x00, 0x00, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x64 };

    returnValue = bufferevent_write(clientSessData->bufferEvent, data, 15);
    return returnValue;
}

/// Gets called every time a frame is recieved from a client
/// \param clientSessData - ClientSessionData
/// \return 0 if successful, non-zero if failure

int h2_utils::sessionOnReceived(struct ClientSessionData *clientSessData) {

    /* TODO: Remove readlen, or make it function */
    ssize_t readlen;
    struct evbuffer *in = bufferevent_get_input(clientSessData->bufferEvent);
    size_t length = evbuffer_get_length(in);
    unsigned char *data = evbuffer_pullup(in, -1); // Make whole buffer contiguous

    h2_frame_handlers::frameHandler(clientSessData, data, length);

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


/// Creates a clientSessionData object which keeps all to be used for one single connection.
///
/// @param appCtx - Application-wide application_ctx object.
/// @param sock - file descriptor for the connection
/// @param clientAddress - socket address of the client.
/// @param addressLength - length of socket address.
/// @return clientSessData - the newly created

struct ClientSessionData *h2_utils::createClientSessionData(struct ApplicationContext *appCtx, int sock, struct sockaddr *clientAddress,
                                          int addressLength) {
    int returnValue;
    struct ClientSessionData *clientSessData;
    SSL *ssl;
    char host[1025];
    int val = 1;

    /* Create new TLS session object */
    ssl = SSL_new(appCtx->ctx);
    if (!ssl) {
        errx(1, "Could not create TLS session object: %s",
             ERR_error_string(ERR_get_error(), nullptr));
    }

    clientSessData = (ClientSessionData *)malloc(sizeof(ClientSessionData));
    memset(clientSessData, 0, sizeof(ClientSessionData));

    clientSessData->appCtx = appCtx;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val)); // set TCP_NODELAY option for improved latency

    clientSessData->bufferEvent = bufferevent_openssl_socket_new(
            appCtx->eventBase, sock, ssl, BUFFEREVENT_SSL_ACCEPTING,
            BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS
    );

    bufferevent_enable(clientSessData->bufferEvent, EV_READ | EV_WRITE);

    returnValue = getnameinfo(clientAddress, (socklen_t)addressLength, host, sizeof(host), nullptr, 0, NI_NUMERICHOST);

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

/// Converts bytes from unsigned char* to string
/// \param data - bytes to be converted
/// \param firstIndex - Index to start reading from
/// \param secondIndex - Index after the last to byte to be read
/// \return string - from bytes

string h2_utils::bytesToString(const unsigned char *data, size_t firstIndex, size_t secondIndex) {
    static const char characters[] = "0123456789abcdef";

    std::string res((secondIndex - firstIndex) * 2, 0);

    char *buf = (res.data());

    for (size_t i = firstIndex; i < secondIndex; ++i) {
        *buf++ = characters[data[i] >> 4];
        *buf++ = characters[data[i] & 0x0F];
    }

    return res;
}


/// Convert hexString to ulong
/// \param hexString - hexString to be converted
/// \return ulong

ulong h2_utils::hexToUlong(string hexString) {
    ulong hexDecimalValue;
    if (hexString[0] == '0' && hexString[1] == 'x') {
        istringstream iss(hexString);
        iss >> hex >> hexDecimalValue;
    } else {
        string hex = "0x" + hexString;
        istringstream iss(hex);
        iss >> std::hex >> hexDecimalValue;
    }
    return hexDecimalValue;
}

#define MAKE_NV(K, V)                                                          \
  {                                                                            \
    (uint8_t *)K, (uint8_t *)V, sizeof(K) - 1, sizeof(V) - 1,                  \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }


/// Send a get response when recieving a request
///
/// @param ClientSessData - ClientSessionData
/// @param data - pointer to array with the request
/// @param path - where to find requested resource

void h2_utils::sendGetResponse(ClientSessionData *ClientSessData, const unsigned char *data, string path) {
    string filepath = resolvePath(ClientSessData, std::move(path));
    if (!filepath.empty()) {
        getResponse200(ClientSessData, data, filepath);
    } else {
        getResponse404(ClientSessData, data);
    }
}


/// Resolves user-defined routes, and returns the filepath in publicDir
///
/// @param clientSessData - ClientSessionData to access list of routes
/// @param path - path to resolve
/// @return string - filepath in publicDir

string h2_utils::resolvePath(ClientSessionData *clientSessData, string path) {
    string filepath = publicDir + path;
    ifstream in(filepath);
    //cout << filepath << endl;

    if (!clientSessData->appCtx->routes[path].empty()) {
        filepath = publicDir + clientSessData->appCtx->routes[path];
        return filepath;
    } else if (in) {
        // TODO: Handle different file types (.css and .js)
        return filepath;
    } else {
        return "";
    }
}


/// Responds to get request with html, js or css file, and status: 200.
///
/// @param clientSessData - ClientSessionData, used to respond to correct client
/// @param data - pointer to array containing request
/// @param filepath - where the requested file is located

void h2_utils::getResponse200(struct ClientSessionData *clientSessData, const unsigned char *data, string filepath) {
    regex filetype_regex(".{1}\\w+$");
    auto filetype_found = sregex_iterator(filepath.begin(), filepath.end(), filetype_regex);
    smatch match = *filetype_found;
    string filetype = match.str();

    // HEADER FRAME:
    ssize_t rv;

    nghttp2_nv nvaHTML[] = {
            MAKE_NV(":status", "200"),
            MAKE_NV("content-type", "text/html;charset=UTF-8")
    };

    nghttp2_nv nvaCSS[] = {
            MAKE_NV(":status", "200"),
            MAKE_NV("content-type", "text/css;charset=UTF-8")
    };

    nghttp2_nv nvaJS[] = {
            MAKE_NV(":status", "200"),
            MAKE_NV("content-type", "text/javascript;charset=UTF-8")
    };

    uint8_t *buf;
    size_t buflen;
    size_t outlen;
    size_t i;
    size_t sum = 0;

    if (filetype == ".html") {
        size_t nvlen = sizeof(nvaHTML) / sizeof(nvaHTML[0]);

        for (i = 0; i < nvlen; ++i) {
            sum += nvaHTML[i].namelen + nvaHTML[i].valuelen;
        }

        //printf("Input (%zu byte(s)):\n\n", sum);
        if (printFrames) {
            cout << "\nHEADER FIELDS SENT:" << endl;

            for (i = 0; i < nvlen; ++i) {
                fwrite(nvaHTML[i].name, 1, nvaHTML[i].namelen, stdout);
                printf(": ");
                fwrite(nvaHTML[i].value, 1, nvaHTML[i].valuelen, stdout);
                printf("\n");
            }
            printf("\n");
        }

        buflen = nghttp2_hd_deflate_bound(clientSessData->deflater, nvaHTML, nvlen);
        buf = static_cast<uint8_t *>(malloc(buflen));

        rv = nghttp2_hd_deflate_hd(clientSessData->deflater, buf, buflen, nvaHTML, nvlen);
    } else if (filetype == ".css") {
        size_t nvlen = sizeof(nvaCSS) / sizeof(nvaCSS[0]);

        for (i = 0; i < nvlen; ++i) {
            sum += nvaCSS[i].namelen + nvaCSS[i].valuelen;
        }

        //printf("Input (%zu byte(s)):\n\n", sum);
        if (printFrames) {
            cout << "\nHEADER FIELDS SENT:" << endl;

            for (i = 0; i < nvlen; ++i) {
                fwrite(nvaCSS[i].name, 1, nvaCSS[i].namelen, stdout);
                printf(": ");
                fwrite(nvaCSS[i].value, 1, nvaCSS[i].valuelen, stdout);
                printf("\n");
            }
            printf("\n");
        }

        buflen = nghttp2_hd_deflate_bound(clientSessData->deflater, nvaCSS, nvlen);
        buf = static_cast<uint8_t *>(malloc(buflen));

        rv = nghttp2_hd_deflate_hd(clientSessData->deflater, buf, buflen, nvaCSS, nvlen);
    } else if (filetype == ".js") {
        size_t nvlen = sizeof(nvaJS) / sizeof(nvaJS[0]);

        for (i = 0; i < nvlen; ++i) {
            sum += nvaJS[i].namelen + nvaJS[i].valuelen;
        }

        //printf("Input (%zu byte(s)):\n\n", sum);
        if (printFrames) {
            cout << "\nHEADER FIELDS SENT:" << endl;

            for (i = 0; i < nvlen; ++i) {
                fwrite(nvaJS[i].name, 1, nvaJS[i].namelen, stdout);
                printf(": ");
                fwrite(nvaJS[i].value, 1, nvaJS[i].valuelen, stdout);
                printf("\n");
            }
            printf("\n");
        }

        buflen = nghttp2_hd_deflate_bound(clientSessData->deflater, nvaJS, nvlen);
        buf = static_cast<uint8_t *>(malloc(buflen));

        rv = nghttp2_hd_deflate_hd(clientSessData->deflater, buf, buflen, nvaJS, nvlen);
    } else {
        return;
    }

    if (rv < 0) {
        fprintf(stderr, "nghttp2_hd_deflate_hd() failed with error: %s\n",
                nghttp2_strerror((int) rv));

        free(buf);

        exit(EXIT_FAILURE);
    }

    outlen = (size_t) rv;
    //cout << "outlen: " << outlen << endl;

    size_t outlen1 = outlen;
    size_t outlen2 = 0;
    size_t outlen3 = 0;
    if (outlen < 16777216) {       // Big enough to need 3 bytes to represent length
        outlen3 = outlen / 65536;
        outlen2 = (outlen % 65536) / 256;
        outlen1 = outlen % 256;
        // cout << outlen3 << "\t" << outlen2 << "\t" << outlen1 << endl;
    } else {                                            // Big enough to need more than 1 frame
        // TODO: Handle payloads, long enough to need more than 1 frame
    }

    unsigned char frameHeader[] = {(unsigned char) outlen3, (unsigned char) outlen2, (unsigned char) outlen1,     // Length
                                   Types::HEADERS,                         // Type
                                   END_HEADERS,                            // Flags
                                   data[5], data[6], data[7], data[8]};               // Stream-ID
    auto *frame = new unsigned char[9 + outlen];

    for (i = 0; i < 9; ++i) frame[i] = frameHeader[i];
    for (i = 0; i < outlen; ++i) frame[i + 9] = buf[i];

    bufferevent_write(clientSessData->bufferEvent, frame, outlen + 9);

    // DATA FRAME:
    ifstream in(filepath);
    string s = string((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    auto sLen = s.length();
    //cout << "sLen: " << sLen << endl;

    size_t sLen1 = sLen;
    size_t sLen2 = 0;
    size_t sLen3 = 0;
    if (sLen < 16777216) {       // Big enough to need 3 bytes to represent length
        sLen3 = sLen / 65536;
        sLen2 = (sLen % 65536) / 256;
        sLen1 = sLen % 256;
        // cout << sLen3 << "\t" <<sLen2 << "\t" << sLen1 << endl;
    } else {                                            // Big enough to need more than 1 frame
        // TODO: Handle payloads, long enough to need more than 1 frame
    }

    unsigned char frameHeader2[] = {(unsigned char) sLen3, (unsigned char) sLen2, (unsigned char) sLen1,   // Length
                                    DATA,                                                                   // Type
                                    END_STREAM,                                                             // Flags
                                    data[5], data[6], data[7], data[8]};                                    // Stream-ID
    auto *frame2 = new unsigned char[9 + sLen];

    for (i = 0; i < 9; ++i) frame2[i] = frameHeader2[i];
    for (i = 0; i < sLen; ++i) frame2[i + 9] = (unsigned char) s[i];

    bufferevent_write(clientSessData->bufferEvent, frame2, sLen + 9);
}


/// Sends a status: 404 Not Found response
///
/// @param clientSessData - ClientSessionData, used to respond to correct client
/// @param data - pointer to array containing request

void h2_utils::getResponse404(struct ClientSessionData *clientSessData, const unsigned char *data) {
    // HEADER FRAME:
    ssize_t rv;

    nghttp2_nv nva[] = {
            MAKE_NV(":status", "404")
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

    if (printFrames) {
        //printf("Input (%zu byte(s)):\n\n", sum);
        cout << "\n\nHEADER FIELDS SENT:" << endl;

        for (i = 0; i < nvlen; ++i) {
            fwrite(nva[i].name, 1, nva[i].namelen, stdout);
            printf(": ");
            fwrite(nva[i].value, 1, nva[i].valuelen, stdout);
            printf("\n");
        }
        printf("\n");
    }

    buflen = nghttp2_hd_deflate_bound(clientSessData->deflater, nva, nvlen);
    buf = static_cast<uint8_t *>(malloc(buflen));

    rv = nghttp2_hd_deflate_hd(clientSessData->deflater, buf, buflen, nva, nvlen);

    if (rv < 0) {
        fprintf(stderr, "nghttp2_hd_deflate_hd() failed with error: %s\n",
                nghttp2_strerror((int) rv));

        free(buf);

        exit(EXIT_FAILURE);
    }

    outlen = (size_t) rv;
    //cout << "outlen: " << outlen << endl;

    unsigned char frameHeader[] = {0x00, 0x00, (unsigned char) outlen,     // Length
                                   Types::HEADERS,                         // Type
                                   END_HEADERS | END_STREAM,                            // Flags
                                   data[5], data[6], data[7], data[8]};               // Stream-ID
    auto *frame = new unsigned char[9 + outlen];

    for (i = 0; i < 9; ++i) frame[i] = frameHeader[i];
    for (i = 0; i < outlen; ++i) frame[i + 9] = buf[i];

    bufferevent_write(clientSessData->bufferEvent, frame, outlen + 9);
}

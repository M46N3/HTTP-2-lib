// h2_utils.hpp

#pragma once
#include <stddef.h>
#include <string>
#include "h2_structs.hpp"

using namespace std;

class h2_utils {
public:
    static int sendConnectionHeader(struct ClientSessionData *clientSessData);
    static int sessionOnReceived(struct ClientSessionData *clientSessData);
    static struct ClientSessionData *createClientSessionData(struct ApplicationContext *appCtx, int sock, struct sockaddr *clientAddress, int addressLength);
    static string bytesToString(const unsigned char *data, size_t firstIndex, size_t secondIndex);
    static ulong hexToUlong(string hexString);
    static void sendGetResponse(struct ClientSessionData *clientSessData, const unsigned char *data, string path);
    static string resolvePath(struct ClientSessionData *clientSessData, string path);
    static void getResponse200(struct ClientSessionData *clientSessData, const unsigned char *data, string filepath);
    static void getResponse404(struct ClientSessionData *clientSessData, const unsigned char *data);
    static void deleteClientSessionData(struct ClientSessionData *clientSessData);
};
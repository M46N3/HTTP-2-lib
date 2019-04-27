// h2_frame_handlers.hpp

#pragma once
#include <stddef.h>
#include <string.h>
#include "h2_structs.hpp"

class h2_frame_handlers {
public:
    static void frameHandler(struct ClientSessionData *clientSessData, const unsigned char *data, size_t length);
    static void frameDefaultPrint(const unsigned char *data);
    static void headerFrameHandler(struct ClientSessionData *clientSessData, const unsigned char *data, size_t length);
    static void settingsFrameHandler(struct ClientSessionData *clientSessData, const unsigned char *data, size_t length);
    static void windowUpdateFrameHandler(struct ClientSessionData *clientSessData, const unsigned char *data, size_t length);
};
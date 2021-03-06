// h2_frame_handlers.cpp

#include "h2_frame_handlers.hpp"
#include "h2_utils.hpp"
#include "h2_enums.hpp"
#include <event2/bufferevent_ssl.h>
#include "h2_global.hpp"


/// Handles frame correctly depening on type of frame
/// \param clientSessData - ClientSessionData
/// \param data - the frame data
/// \param length - length of the frame

void h2_frame_handlers::frameHandler(struct ClientSessionData *clientSessData, const unsigned char *data, size_t length) {
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
            string dataString = h2_utils::bytesToString(data, 0, 24);

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
                            ulong nextFrameLength = h2_utils::hexToUlong(h2_utils::bytesToString(data, currentPos, (currentPos + 3)));
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


/// Prints the frameheader from a frame (first 9 octets)
/// \param data - frame to print frameheader from

void h2_frame_handlers::frameDefaultPrint(const unsigned char *data) {
    // Printing Frame Format
    if (printFrames) {
        for (size_t i = 0; i < 9; ++i) {
            if (i == 0) cout << "Length(24):\t\t\t\t\t";
            if (i == 3) {
                cout << "\t\t" << h2_utils::hexToUlong(h2_utils::bytesToString(data, (0), (3))) << " octets";
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
            if (i == 8) cout << "\t" << h2_utils::hexToUlong(h2_utils::bytesToString(data, 5, 9));
        }
    }
}


/// Handles frame of type: HEADER
/// \param clientSessData - ClientSessionData
/// \param data - the frame data
/// \param length - length of the frame

void h2_frame_handlers::headerFrameHandler(ClientSessionData *clientSessData, const unsigned char *data, size_t length) {
    // Checks if the padded and priority flags for the header frame are set
    const bool padded = bitset<8>(data[4])[3];      // PADDED = 0x8
    const bool priority = bitset<8>(data[4])[5];    // PRIORITY = 0x20

    if (printFrames) cout << "\nPayload:";

    // HPACK decoding:
    ssize_t rv;

    ulong padlength = 0;
    if (padded) {
        padlength = bitset<8>(data[9]).to_ulong();
    }

    auto *in = (uint8_t*)data + 9 + (priority ? 5 : 0) + (padded ? 1 : 0);
    size_t inlen = length - 9 - (priority ? 5 : 0) - padlength;

    string method;
    string path;

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
            if (printFrames) printf("\n%s : %s", (char*) nv.name, (char*) nv.value);
            string name = (char*) nv.name;
            string value = (char*) nv.value;
            if (name == ":method") method = value;
            if (name == ":path") path = value;
        }

        if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
            nghttp2_hd_inflate_end_headers(clientSessData->inflater);
            break;
        }

        if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && inlen == 0) {
            break;
        }
    }
    //cout << "\nMethod: " << method << ", Path: " << path << endl;
    if (method == "GET") {
        h2_utils::sendGetResponse(clientSessData, data, path);
    }
}


/// Handles frame of type: SETTINGS
/// \param clientSessData - ClientSessionData
/// \param data - the frame data
/// \param length - length of the frame

void h2_frame_handlers::settingsFrameHandler(ClientSessionData *clientSessData, const unsigned char *data, size_t length) {
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
                cout << "\t" << h2_utils::hexToUlong(h2_utils::bytesToString(data, (valuePrint-3), (valuePrint+1)));
                valuePrint += 6;
            }
        }
        cout << endl << endl;
    }
    const bool acknowledge = bitset<8>(data[4])[0];      // ACK = 0x1
    ulong payloadLength = h2_utils::hexToUlong(h2_utils::bytesToString(data, 0, 3));


    //    TODO: Do more than just acknowledge the settings frame.
    //          Save data from settings frame to the ClientSession object for the connection.
    if (!acknowledge) {
        unsigned char ackSettingsFrame[] = { 0x00, 0x00, 0x00, Types::SETTINGS, 0x01, 0x00, 0x00, 0x00, 0x00 };
        bufferevent_write(clientSessData->bufferEvent, ackSettingsFrame, 9);
    }

    if (acknowledge && payloadLength != 0) {
        if (printFrames) {
            cout << "GOAWAY" << endl << endl;
        }
        unsigned char goawayFrame[] = { 0x00, 0x00, 0x08, Types::GOAWAY, 0x00,
                                        0x00, 0x00, 0x00, 0x00,
                                        data[5], data[6], data[7], data[8],
                                        0x00, 0x00, 0x00, ErrorCodes::FRAME_SIZE_ERROR };
        bufferevent_write(clientSessData->bufferEvent, goawayFrame, 17);
    }
}


/// Handles frame of type: PING
/// \param clientSessData - ClientSessionData
/// \param data - the frame data
/// \param length - length of the frame

void h2_frame_handlers::pingFrameHandler(ClientSessionData *clientSessData, const unsigned char *data, size_t length) {
    const bool ack = bitset<8>(data[4])[0];      // ACK = 0x1

    // Respond to ping if ack flag is not set
    if (!ack) {
        auto *response = new unsigned char[length];
        for (size_t i = 0; i < length; ++i) response[i] = data[i];
        response[4] = 0x1;
        bufferevent_write(clientSessData->bufferEvent, response, length);
        if (printFrames) cout << "\nResponded to PING frame" << endl;
    } else {
        if (printFrames) cout << "\nRevieved PING response" << endl;
    }
}


/// Handles frame of type: WINDOW_UPDATE
/// \param clientSessData - ClientSessionData
/// \param data - the frame data
/// \param length - length of the frame

void h2_frame_handlers::windowUpdateFrameHandler(ClientSessionData *clientSessData, const unsigned char *data, size_t length) {
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
                cout << "\t" << h2_utils::hexToUlong(h2_utils::bytesToString(data, (valuePrint-3), (valuePrint+1))) << " octets??";
                valuePrint += 4;
            }
        }
        cout << endl << endl;
    }
}
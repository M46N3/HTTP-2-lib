#include <cstdio>
#include <bitset>
#include <iostream>
#include <string.h>

using namespace std;

enum Types {
    DATA = 0x0,
    HEADERS = 0x1,
    PRIORITY = 0x2,
    RST_STREAM = 0x3,
    SETTINGS = 0x4,
    PUSH_PROMISE = 0x5,
    PING = 0x6,
    GOAWAY = 0x7,
    WINDOW_UPDATE = 0x8,
    CONTINUATION = 0x9
};

enum SettingsParameters {
    SETTINGS_HEADER_TABLE_SIZE = 0x1,
    SETTINGS_ENABLE_PUSH = 0x2,
    SETTINGS_MAX_CONCURRENT_STREAMS = 0x3,
    SETTINGS_INITIAL_WINDOW_SIZE = 0x4,
    SETTINGS_MAX_FRAME_SIZE = 0x5,
    SETTINGS_MAX_HEADER_LIST_SIZE = 0x6
};

struct frame {
    bitset<24> length;
    bitset<8> type;
    bitset<8> flags;
    bitset<1> r;
    bitset<31> stream_id;
} data1, headers, priority, rst_stream, settings, push_promise, ping, goaway, window_update, continuation;

char* getBytes(frame frame);
char* getBytesString(frame frame, const string &payload, size_t len);

char* dataframe(bitset<8> flags, bitset<31> stream_id, const string &payload, size_t len) {
    data1.length = len;
    data1.type = Types::DATA;
    data1.flags = flags;
    data1.r = bitset<1>(0x0);
    data1.stream_id = stream_id;

    return getBytesString(data1, payload, len);
}

char* settingsframe(bitset<8> flags, bitset<31> stream_id) {
    settings.length = bitset<24>(0x0);
    settings.type = Types::SETTINGS;
    settings.flags = flags;
    settings.r = bitset<1>(0x0);
    settings.stream_id = stream_id;

    /*cout << settings.length << endl;
    cout << settings.type << endl;
    cout << settings.flags << endl;
    cout << settings.r << endl;
    cout << settings.stream_id << endl;
    cout << getAll(settings) << endl;*/

    return getBytes(settings);
}

char* getBytes(frame frame) {
    string s = frame.length.to_string() + frame.type.to_string() + frame.flags.to_string() + frame.r.to_string() + frame.stream_id.to_string();
    char* chars = new char[9];
    //string s = bs.to_string();
    for (size_t i = 0; i < 9; i++) {
        bitset<8> b(s, (i*8), 8);
        chars[i] = (b.to_ulong() & 0xFF);
        //cout << static_cast<int>(chars[i]);
    }
    return chars;
}

char* getBytesString(frame frame, const string &payload, size_t len) {
    string s = frame.length.to_string() + frame.type.to_string() + frame.flags.to_string() + frame.r.to_string() + frame.stream_id.to_string();
    char* chars = new char[9 + len];
    //string s = bs.to_string();


    for (char i : payload) {
        //cout << bitset<8>(payload[i]) << endl;
        s += bitset<8>(i).to_string();
    }

    //cout << s << endl;
    for (size_t i = 0; i < (9 + len); i++) {
        bitset<8> b(s, (i*8), 8);
        chars[i] = (b.to_ulong() & 0xFF);
        //cout << static_cast<int>(chars[i]);
    }
    return chars;
}

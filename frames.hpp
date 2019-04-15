#include <cstdio>
#include <bitset>
#include <iostream>
#include <string.h>

using namespace std;

enum Types {
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

struct frame {
    bitset<24> length;
    bitset<8> type;
    bitset<8> flags;
    bitset<1> r;
    bitset<31> stream_id;
    //bitset<length> payload;
} headers, priority, rst_stream, settings, push_promise, ping, goaway, window_update, continuation;

char* getBytes(frame frame);

char* settingsframe(bitset<8> flags) {
    settings.length = bitset<24>(0x0);
    settings.type = Types::SETTINGS;
    settings.flags = flags;
    settings.r = bitset<1>(0x0);
    settings.stream_id = bitset<31>(0x0);

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
    for (int i = 0; i < 9; i++) {
        bitset<8> b(s, (i*8), 8);
        chars[i] = (b.to_ulong() & 0xFF);
        cout << static_cast<int>(chars[i]);
    }
    return chars;
}
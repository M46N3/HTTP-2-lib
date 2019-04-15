#include <cstdio>
#include <bitset>
#include <iostream>
#include <string.h>

using namespace std;

enum types {

};

struct frame {
    bitset<24> length;
    bitset<8> type;
    bitset<8> flags;
    bitset<1> r;
    bitset<31> stream_id;
    //bitset<length> payload;
} settings, header;

bitset<72> getAll(frame frame);

char* getBytes(bitset<72> bs);

char* settingsframe(bitset<8> flags) {
    settings.length = bitset<24>(0x0);
    settings.type = bitset<8>(0x4);
    settings.flags = flags;
    settings.r = bitset<1>(0x0);
    settings.stream_id = bitset<31>(0x0);

    cout << settings.length << endl;
    cout << settings.type << endl;
    cout << settings.flags << endl;
    cout << settings.r << endl;
    cout << settings.stream_id << endl;

    cout << getAll(settings) << endl;
    return getBytes(getAll(settings));
}

bitset<72> getAll(frame frame) {
    string res = frame.length.to_string() + frame.type.to_string() + frame.flags.to_string() + frame.r.to_string() + frame.stream_id.to_string();
    return bitset<72>(res);
}

char* getBytes(bitset<72> bs) {
    char* chars = new char[9];
    string s = bs.to_string();
    for (int i = 0; i < 9; i++) {
        bitset<8> b(s, (i*8), 8);
        chars[i] = (b.to_ulong() & 0xFF);
        //cout << static_cast<int>(chars[i]);
    }
    return chars;
}
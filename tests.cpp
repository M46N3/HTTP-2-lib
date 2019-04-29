#define BOOST_TEST_MODULE boost_test_macro_overview
#include <boost/test/included/unit_test.hpp>
#include "h2_utils.hpp"
#include <string>

using namespace std;

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

BOOST_AUTO_TEST_CASE( test_bytesToString) {
        namespace tt = boost::test_tools;

        // bytesToString
        unsigned char testData[] = {0x4, 0x45, 0x8, 0x54};
        string testString = string("04450854");
        string result = h2_utils::bytesToString(testData, 0, 4);

        // hexToUlong
        ulong result2 = h2_utils::hexToUlong(testString);
        ulong solution2 = 71632980;


        BOOST_TEST(result == testString);
        BOOST_TEST(result2 == solution2);
}


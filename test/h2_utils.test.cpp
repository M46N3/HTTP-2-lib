#include <boost/test/unit_test.hpp>
#include "../src/h2_utils.hpp"

using namespace std;


BOOST_AUTO_TEST_SUITE( test_suite_h2_utils )

BOOST_AUTO_TEST_CASE( test_bytesToString) {
        unsigned char testData[] = {0x4, 0x45, 0x8, 0x54};
        string testString = string("04450854");
        string result = h2_utils::bytesToString(testData, 0, 4);

        BOOST_TEST(result == "04450854");
        BOOST_TEST(result != "0445854");
}

BOOST_AUTO_TEST_CASE( test_hexToUlong ) {
    string testString = string("04450854");
    ulong result = h2_utils::hexToUlong(testString);
    ulong correctUlong = 71632980L;
    ulong incorrectUlong = 71632981L;

    BOOST_TEST(result == correctUlong);
    BOOST_TEST(result != incorrectUlong);
}

BOOST_AUTO_TEST_SUITE_END()
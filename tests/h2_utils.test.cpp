#define BOOST_TEST_MODULE h2_utils.test.cpp
#include <boost/test/included/unit_test.hpp>
#include "../h2_utils.hpp"
#include "../h2_global.hpp"
#include <string>
#include <sstream>

using namespace std;

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

BOOST_AUTO_TEST_CASE( test_setPublicDir ) {
    string dir = string("./my_public_dir");
    h2_utils::setPublicDir(dir);

    BOOST_TEST(publicDir == dir);
}

BOOST_AUTO_TEST_CASE( test_addRoute ) {
    string path1 = string("/my_path");
    string filepath1 = string("my_file.html");
    string path2 = string("/my_second_path");
    string filepath2 = string("my_second_file.html");
    h2_utils::addRoute(&appCtx, path1, filepath1);
    h2_utils::addRoute(&appCtx, path2, filepath2);

    BOOST_TEST(appCtx.routes[path1] == filepath1);
    BOOST_TEST(appCtx.routes[path2] == filepath2);
    BOOST_TEST(appCtx.routes[path1] != filepath2);
    BOOST_TEST(appCtx.routes[path2] != filepath1);
}
#include <boost/test/unit_test.hpp>
#include "../include/HTTP-2-lib/h2_server.hpp"
#include "../src/h2_global.hpp"

BOOST_AUTO_TEST_SUITE(test_suite_h2_server)

BOOST_AUTO_TEST_CASE( test_setPublicDir ) {
        string dir = string("./my_public_dir");
        h2_server::setPublicDir(dir);

        BOOST_TEST(publicDir == dir);
        BOOST_TEST(publicDir != "./not_my_public_dir");
}

BOOST_AUTO_TEST_CASE( test_addRoute ) {
        string path1 = string("/my_path");
        string filepath1 = string("my_file.html");
        string path2 = string("/my_second_path");
        string filepath2 = string("my_second_file.html");
        h2_server::addRoute(path1, filepath1);
        h2_server::addRoute(path2, filepath2);

        BOOST_TEST(appCtx.routes[path1] == filepath1);
        BOOST_TEST(appCtx.routes[path2] == filepath2);
        BOOST_TEST(appCtx.routes[path1] != filepath2);
        BOOST_TEST(appCtx.routes[path2] != filepath1);
}

BOOST_AUTO_TEST_SUITE_END()
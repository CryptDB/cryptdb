#pragma once

/*
 * TestCrypto.h
 *
 *  Created on: Jul 15, 2011
 *      Author: cat_red
 */


#include <test/test_utils.hh>

class TestCrypto {
 public:
    TestCrypto();
    virtual
    ~TestCrypto();

    static void run(const TestConfig &tc, int argc, char ** argv);
};

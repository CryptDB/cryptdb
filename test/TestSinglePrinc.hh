#pragma once

/*
 * TestSinglePrinc.h
 *
 *  Created on: Jul 5, 2011
 *      Author: raluca
 */

#include <main/EDBProxy.hh>
#include <test/test_utils.hh>


class TestSinglePrinc {
 public:
    TestSinglePrinc();
    virtual
    ~TestSinglePrinc();

    static void run(const TestConfig &tc, int argc, char ** argv);
};

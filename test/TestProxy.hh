#pragma once

/*
 * TestProxy.h
 *
 * Created on: August 3, 2011
 *    Author: cat_red
 */

#include <signal.h>

#include <main/Connect.hh>
#include <test/test_utils.hh>


class TestProxy {
 public:
    TestProxy();
    virtual
    ~TestProxy();

    static void run(const TestConfig &tc, int argc, char ** argv);
};

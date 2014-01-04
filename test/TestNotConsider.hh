#pragma once

/*
 * TestNotConsider.h
 */

#include <main/EDBProxy.hh>
#include <test/test_utils.hh>


class TestNotConsider {
 public:
    TestNotConsider();
    ~TestNotConsider();

    static void run (const TestConfig &tc, int argc, char ** argv);
};

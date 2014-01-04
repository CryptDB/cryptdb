#pragma once

/*
 * TestAccessManager.h
 *
 * Created on: July 21, 2011
 *  Author: cat_red
 */

#include <main/AccessManager.hh>
#include <test/test_utils.hh>


class TestAccessManager {
 public:
  TestAccessManager();
  virtual
    ~TestAccessManager();

  static void run(const TestConfig &tc, int argc, char ** argv);
};

#pragma once

#include <sys/time.h>

class timer {
 private:
    timer(const timer &t);  /* no reason to copy timer objects */

 public:
    timer() { lap(); }

    uint64_t lap() {        /* returns microseconds */
        uint64_t t0 = start;
        uint64_t t1 = cur_usec();
        start = t1;
        return t1 - t0;
    }

 private:
    static uint64_t cur_usec() {
        struct timeval tv;
        gettimeofday(&tv, 0);
        return ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
    }

    uint64_t start;
};

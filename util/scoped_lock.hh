#pragma once

#include <pthread.h>

class scoped_lock {
 public:
    scoped_lock(pthread_mutex_t *muarg) : mu(muarg) {
        pthread_mutex_lock(mu);
    }

    ~scoped_lock() {
        pthread_mutex_unlock(mu);
    }

 private:
    pthread_mutex_t *mu;
};

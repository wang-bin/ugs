/*
 * Copyright (c) 2016 WangBin <wbsecg1 at gmail.com>
 */
#include "Semaphore.h"
#include <cassert>
#include <chrono>

using namespace std;
Semaphore::Semaphore(int n)
    : n_(n)
{
    assert(n >= 0 && "Semaphore n must >= 0");
}

void Semaphore::acquire(int n)
{
    assert(n >= 0 && "Semaphore::acquire n must >= 0");
    std::unique_lock<std::mutex> lock(mutex_);
    while (n > n_)
        cond_.wait(lock);
    n_ -= n;
}

bool Semaphore::tryAcquire(int n, int timeout)
{
    assert(n >= 0 && "Semaphore::tryAcquire n must >= 0");
    std::unique_lock<std::mutex> lock(mutex_);
    if (timeout == 0) {
        if (n > n_)
            return false;
        n_ -= n;
        return true;
    }
    if (timeout < 0) {
        while (n > n_)
            cond_.wait(lock);
    } else {
        auto t0 = chrono::system_clock::now();
        while (n > n_) {
            const int64_t dt = timeout - chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - t0).count();
            if (dt < 0 || cond_.wait_for(lock, chrono::milliseconds(dt)) == cv_status::timeout)
                return false;
        }
    }
    n_ -= n;
    return true;
}

void Semaphore::release(int n)
{
    assert(n >= 0 && "Semaphore::release n must >= 0");
    std::lock_guard<std::mutex> lock(mutex_);
    n_ += n;
    cond_.notify_all();
}

int Semaphore::available() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return n_;
}

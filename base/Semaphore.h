/*
 * Copyright (c) 2016 WangBin <wbsecg1 at gmail.com>
 */
#pragma once
// TODO: use platform semaphore API
#include <mutex>
#include <condition_variable>

class Semaphore
{
public:
    explicit Semaphore(int n = 0);

    void acquire(int n = 1);
    /*!
     * \brief tryAcquire
     * \param n
     * \param timeout in ms
     * \return
     */
    bool tryAcquire(int n = 1, int timeout = 0);
    void release(int n = 1);
    int available() const;
private:
    Semaphore(const Semaphore &) = delete;
    Semaphore &operator=(const Semaphore &) = delete;

    int n_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};

/*
 * Copyright (c) 2012-2017 WangBin <wbsecg1 at gmail.com>
 * Original code is from QtAV project
 */
#pragma once

#include <algorithm>
#include <functional>
#include <mutex>
#include <list>
#include <condition_variable>
#include <utility>
template <typename T, template <typename...> class C = std::list> // TODO: use std::list by default. queue is just a wrapper
/*!
 * \brief The BlockingQueue class
 * |0-------------|min+1------------|wakePush----------|max
 * block pop      wake pop         wake push          block push
 * use min+1 as wake_pop_: size() <= min_ implies 1. min_ > 0 but not enough, 2. min_ == 0 && queue_.empty()
 * call setWaitForMin(false) to pop even if min is not reached.
 */
class BlockingQueue { // TODO: rename BlockingFIFO
public:
    typedef T type; // requirements: 1. T has default ctor
    typedef C<T, std::allocator<T>> queue_t;  // vs2013: C2976 if no allocator<T>
    using iterator = typename queue_t::iterator;
    using const_iterator = typename queue_t::const_iterator;
    /*!
     * \brief BlockingQueue
     * \param max maximum elements the queue can hold. push will be blocked or failed
     * \param min cache value. If queue is blocked (pop when empty), keep blocking until size >= min
     */
    BlockingQueue(size_t max = std::numeric_limits<size_t>::max(), size_t min = 0)
        : min_(min), max_(max), wake_push_(max)
    {}
    BlockingQueue(BlockingQueue const&) = delete;
    BlockingQueue& operator=(BlockingQueue const&) = delete;

    void setMax(size_t value) {
        std::unique_lock<std::mutex> lock(mutex_);
        max_ = value;
        if (queue_.size() < max_)
            full_.notify_one();
    }
    size_t max() const { return max_;}
    /// TODO: rename setWakePop?
    void setMin(size_t value) {
        std::unique_lock<std::mutex> lock(mutex_);
        min_ = value;
        if (queue_.size() > min_)
            empty_.notify_one(); // check size?
    }
    size_t min() const { return min_;}
    void setWaitForMin(bool value = true) { // false: pop returns when not empty ,even if min is not reached
        wait_min_ = value;
    }
    bool popWaiting() const { return pop_waiting_;}
    bool pushWaiting() const { return push_waiting_;}

    /*!
     * \brief setWakePush
     * \param value if wakePush < size <= max, push() is blocked
     */
    void setWakePush(size_t value) { wake_push_ = value;}
    size_t wakePush() const { return wake_push_;}
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_t tmp;
        queue_.swap(tmp);
        full_.notify_all();
        size_ = 0;
        pop_waiting_ = true;
        push_waiting_ = false;
    }

    bool tryPush(T &&t, int64_t timeout = 0) {
        if (size_ >= max_ && timeout <= 0)
            return false;
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= max_) {// TODO: virtual checkFull()
            push_waiting_ = true;
            ///pop_waiting_ = false; // ensure reset without pop
            ///empty_.notify_one();
            if (timeout <= 0 || full_.wait_for(lock, std::chrono::milliseconds(timeout)) == std::cv_status::timeout)
                return false;
        }
        push_waiting_ = false;
        queue_.push_back(std::forward<T>(t));
        ++size_;
        if (size_ > min_ || !wait_min_) {
            pop_waiting_ = false; // ensure reset without pop
            empty_.notify_one();
        }
        return true;
    }
    bool tryPush(const T &t, int64_t timeout = 0) {
        if (size_ >= max_ && timeout <= 0)
            return false;
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= max_) {
            push_waiting_ = true;
            ///pop_waiting_ = false; // ensure reset without pop
            ///empty_.notify_one();
            if (timeout <= 0 || full_.wait_for(lock, std::chrono::milliseconds(timeout)) == std::cv_status::timeout)
                return false;
        }
        push_waiting_ = false; // set after notify_one()?
        queue_.push_back(t);
        ++size_;
        if (size_ > min_ || !wait_min_) {
            pop_waiting_ = false; // ensure reset without pop
            empty_.notify_one();
        }
        return true;
    }
    void push(T &&t) {
        std::unique_lock<std::mutex> lock(mutex_);
        // checkFull, fullCB
        if (queue_.size() >= max_) {
            push_waiting_ = true;
            ///pop_waiting_ = false; // ensure reset without pop
            ///empty_.notify_one();
            full_.wait(lock);
        }
        push_waiting_ = false; // set after notify_one()?
        queue_.push_back(std::forward<T>(t));
        ++size_;
        if (size_ > min_ || !wait_min_)
            empty_.notify_one();
    }
    void push(const T &t) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= max_) {
            push_waiting_ = true;
            ///pop_waiting_ = false; // ensure reset without pop
            ///empty_.notify_one(); // min_ == max_
            full_.wait(lock);
        }
        push_waiting_ = false; // set after notify_one()?
        queue_.push_back(t);
        ++size_;
        if (size_ > min_ || !wait_min_)
            empty_.notify_one();
    }
    size_t tryPop(T &t, int64_t timeout = 0) { // TODO: atomic instead of lock
        if (size_ == 0 && timeout <= 0)
            return 0;
        std::unique_lock<std::mutex> lock(mutex_);
        // checkEmpty, emptyCB
        if (queue_.empty()) {
            pop_waiting_ = true;
            ///push_waiting_ = false; // ensure reset without push();
            ///full_.notify_one();
            if (timeout <= 0 || empty_.wait_for(lock, std::chrono::milliseconds(timeout)) == std::cv_status::timeout)
                return 0;
        }
        const size_t nb = queue_.size();
        pop_waiting_ = nb == 1 && min_ > 0; // set after notify_one()?
        //if (nb == 0)
         //   return 0;
        t = std::move(queue_.front()); //move?
        queue_.pop_front();
        if (--size_ <= wake_push_) {
            push_waiting_ = false; // ensure reset without push();
            full_.notify_one();
        }
        // onPop(t)
        return nb;
    }
    size_t pop(T &t) {
        std::unique_lock<std::mutex> lock(mutex_);
        // checkEmpty, emptyCB
        if (queue_.empty()) {
            pop_waiting_ = true;
            ///push_waiting_ = false; // ensure reset without push();
            ///full_.notify_one();
            empty_.wait(lock);
        }
        if (queue_.empty()) {// FIXME: why it fucking happens?
            pop_waiting_ = true;
            printf("Shit happens. queue is still empty\n");
            empty_.wait(lock);
        }
        const size_t nb = queue_.size();
        pop_waiting_ = nb == 1 && min_ > 0; // set after notify_one()?
        t = std::move(queue_.front()); //move?
        queue_.pop_front();
        if (--size_ <= wake_push_) {
            push_waiting_ = false; // ensure reset without push();
            full_.notify_one();
        }
        return nb;
    }
    size_t size() const { return size_;}
    size_t empty() const { return size() == 0;}

    const T& front() const {
        std::lock_guard<std::mutex> lock(mutex_);
        static T bad;
        if (queue_.empty())
            return bad;
        return queue_.front();
    }
    const T& back() const {
        std::lock_guard<std::mutex> lock(mutex_);
        static T bad;
        if (queue_.empty())
            return bad;
        return queue_.back();
    }
    /*!
     * distance
     * \brief return the distance defined by user, for example elements count, total bytes, duration etc.
     * \param f distance function, parameter it0, it1 is the cbegin() and cend() of container. You may need (it1-1) to access the last element
     */
    template<typename R>
    R distance(std::function<R(const_iterator it0, const_iterator it1)> f) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
            return R(0);
        return f(queue_.cbegin(), queue_.cend());
    }
    // TODO: peak()/at()/[]?
private:
    bool wait_min_ = true;
    bool pop_waiting_ = true;
    bool push_waiting_ = false;
    size_t size_ = 0;
    size_t min_ = 0;
    size_t max_ = std::numeric_limits<size_t>::max();
    size_t wake_push_ = std::numeric_limits<size_t>::max();
    queue_t queue_;
    // compare_op compare_ = nullptr; // require container.insert
    // pop/push cb: for bytes etc. push_cb([&bytes](const Packet& p){ bytes += p.size;})
    mutable std::mutex mutex_;
    std::condition_variable full_;
    std::condition_variable empty_;
};

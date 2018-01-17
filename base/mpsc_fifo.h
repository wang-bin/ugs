/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPSC FIFO
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <utility>

#define MPSC_FIFO_RAW_NEXT_PTR 0

template<typename T>
class mpsc_fifo {
public:
    mpsc_fifo() { in_.store(out_); }

    ~mpsc_fifo() {
        clear();
        delete out_;
    }

    // return number of element cleared
    int clear() {
        int n = 0;
        while (pop())
            n++;
        return n;
    } // in consumer thread

    template<typename... Args>
    void emplace(Args&&... args) {
        node *n = new node{std::forward<Args>(args)...};
#if MPSC_FIFO_RAW_NEXT_PTR // slower
        node* t = in_.load(std::memory_order_relaxed);
        do {
            t->next = n;
        } while (!in_.compare_exchange_weak(t, n, std::memory_order_acq_rel, std::memory_order_relaxed));
#else
        node* t = in_.exchange(n, std::memory_order_acq_rel);
        t->next.store(n, std::memory_order_release);
#endif
    }

    void push(T&& v) {
        node *n = new node{std::move(v)};
#if MPSC_FIFO_RAW_NEXT_PTR // slower
        node* t = in_.load(std::memory_order_relaxed);
        do {
            t->next = n;
        } while (!in_.compare_exchange_weak(t, n, std::memory_order_acq_rel, std::memory_order_relaxed));
#else
        node* t = in_.exchange(n, std::memory_order_acq_rel);
        t->next.store(n, std::memory_order_release);
#endif
    }

    bool pop(T* v = nullptr) {
        // will check next.load() later, also next.store() in push() must be after exchange, so relaxed is enough
        if (out_ == in_.load(std::memory_order_relaxed)) //if (!out_->next) // not completely write to out_->next (t->next.store()), next is not null but invalid
            return false;
#if MPSC_FIFO_RAW_NEXT_PTR
        node *n = out_->next;
#else
        node *n = out_->next.load(std::memory_order_relaxed);
#endif
        if (!n) // before t->next.store() after in_.exchange() in push()
            return false;
        if (v)
            *v = std::move(n->v);
        delete out_;
        out_ = n;
        return true;
    }
private:
    struct node {
        T v;
#if MPSC_FIFO_RAW_NEXT_PTR
        node* next;
#else
        std::atomic<node*> next;
#endif
    };

    node *out_ = new node();
    std::atomic<node*> in_; // can not use in_{out_} because atomic ctor with desired value MUST be constexpr (error in g++4.8 iff use template)
};

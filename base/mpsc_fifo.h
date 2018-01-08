/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 * MIT License
 * Lock Free MPSC FIFO
 * https://github.com/wang-bin/lock_free
 */
#include <atomic>
#include <utility>

template<typename T>
class mpsc_fifo {
public:
    mpsc_fifo() { in_.store(out_); }

    ~mpsc_fifo() {
        while (node* h = out_) {
            out_ = h->next.load();
            delete h;
        }
    }

    void clear() { while(pop()) {}} // in consumer thread

    template<typename... Args>
    void push(Args&&... args) {
        push({std::forward<Args>(args)...});
    }

    void push(T&& v) {
        node *n = new node{std::move(v)};
#if 0
        node* t = in_.load(std::memory_order_relaxed);
        while (!in_.compare_exchange_weak(t, n, std::memory_order_acq_rel, std::memory_order_relaxed)) {}
#else
        node* t = in_.exchange(n, std::memory_order_acq_rel);
#endif
     
        t->next.store(n, std::memory_order_release);
    }

    bool pop(T* v = nullptr) {
        // will check next.load() later, also next.store() in push() must be after exchange, so relaxed is enough
        if (out_ == in_.load(std::memory_order_relaxed)) //if (!head->next) // not completely write to head->next (tail->next), next is not null but invalid
            return false;
        node *n = out_->next.load(std::memory_order_relaxed);
        if (!n)
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
        std::atomic<node*> next;
    };

    node *out_ = new node();
    std::atomic<node*> in_; // can not use in_{out_} because atomic ctor with desired value MUST be constexpr (error in g++4.8 iff use template)
};

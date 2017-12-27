/*
 * Copyright (c) 2017 WangBin <wbsecg1 at gmail.com>
 * Lock Free MPSC FIFO
 */
#include <atomic>
#include <utility>

template<typename T>
class mpsc_fifo {
public:
    ~mpsc_fifo() {
        while (node* h = head_) {
            head_ = h->next.load();
            delete h;
        }
    }
    void push(T&& v) {
        node *n = new node();
        n->v = std::move(v);
#if 0
        node* t = tail_.load(std::memory_order_relaxed);
        while (!tail_.compare_exchange_weak(t, n, std::memory_order_acq_rel, std::memory_order_relaxed)) {}
        t->next.store(n, std::memory_order_release);
#else
        tail_.exchange(n, std::memory_order_acq_rel)->next.store(n, std::memory_order_release);
#endif
    }
    bool pop(T* v = nullptr) {
        // will check next.load() later, also next.store() in push() must be after exchange, so relaxed is enough
        if (head_ == tail_.load(std::memory_order_relaxed)) //if (!head->next) // not completely write to head->next (tail->next), next is not null but invalid
            return false;
        node *n = head_->next.load(std::memory_order_relaxed);
        if (!n)
            return false;
        if (v)
            *v = std::move(n->v);
        delete head_;
        head_ = n;
        return true;
    }
private:
    struct node {
        std::atomic<node*> next;
        T v;
    };
    node *head_ = new node(); // pop_at_
    std::atomic<node*> tail_{head_}; // push_at_
};


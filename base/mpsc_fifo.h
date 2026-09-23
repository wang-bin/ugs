/*
 * Copyright (c) 2017-2026 WangBin <wbsecg1 at gmail.com>
 * AI participated
 * MIT License
 * Lock Free MPSC FIFO
 * https://github.com/wang-bin/lockless
 */
#pragma once
#include <atomic>
#include <type_traits>
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
    size_t clear() noexcept {
        size_t n = 0;
        while (pop())
            n++;
        return n;
    } // in consumer thread

    template<typename... Args>
    void emplace(Args&&... args) {
        node *n = new node{T{std::forward<Args>(args)...}};
        count_++;
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

    template<typename U>
    void push(U&& v) {
        node *n = new node{std::forward<U>(v)};
        count_++;
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

// Consumer-only view of the next value. The pointer remains valid until the consumer calls pop() or clear().
    const T* peek() const noexcept {
        if (out_ == in_.load(std::memory_order_relaxed))
            return nullptr;
#if MPSC_FIFO_RAW_NEXT_PTR
        node* const n = out_->next;
#else
        node* const n = out_->next.load(std::memory_order_acquire);
#endif
        return n ? &n->v : nullptr;
    }

// Snapshot of reserved and published nodes. A producer may still be linking the newest reserved node.
    size_t size() const noexcept {
        return count_.load(std::memory_order_relaxed);
    }

    size_t pop(T* v = nullptr) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>) {
        if (out_ == in_.load(std::memory_order_relaxed)) //if (!out_->next) // not completely write to out_->next (t->next.store()), next is not null but invalid
            return 0;
#if MPSC_FIFO_RAW_NEXT_PTR
        node *n = out_->next;
#else
        // acquire pairs with t->next.store(release) in push(), ensuring the consumer
        // sees the fully-constructed node (including its value) when next is non-null
        node *n = out_->next.load(std::memory_order_acquire);
#endif
        if (!n) // before t->next.store() after in_.exchange() in push()
            return 0;
        // Node n becomes the new sentinel and stays alive until the next pop. Move the
        // value out so a discarded pop/clear() does not retain resources (e.g. HW surfaces).
        T consumed{std::move(n->v)};
        if (v)
            *v = std::move(consumed);
        delete out_;
        out_ = n;
        return count_--;
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
    std::atomic<size_t> count_ = 0;
};

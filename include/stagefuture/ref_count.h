// Copyright (c) 2015-2019 Amanieu d'Antras and DGuco(杜国超).All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef ASYNCXX_H_
# error "Do not include this header directly, include <async++.h> instead."
#endif

namespace stagefuture
{
namespace detail
{

// Default deleter which just uses the delete keyword
template<typename T>
struct default_deleter
{
    static void do_delete(T *p)
    {
        delete p;
        p = nullptr;
    }
};

//template<typename Node, template<typename> class Atom = std::atomic>
//class shared_head_tail_list
//{
//    Atom<Node *> head_;
//    Atom<Node *> tail_;
//
//public:
//    shared_head_tail_list() noexcept
//        : head_(nullptr), tail_(nullptr)
//    {}
//
//    shared_head_tail_list(shared_head_tail_list &&o) noexcept
//    {
//        head_.store(o.head(), std::memory_order_relaxed);
//        tail_.store(o.tail(), std::memory_order_relaxed);
//        o.head_.store(nullptr, std::memory_order_relaxed);
//        o.tail_.store(nullptr, std::memory_order_relaxed);
//    }
//
//    shared_head_tail_list &operator=(shared_head_tail_list &&o) noexcept
//    {
//        head_.store(o.head(), std::memory_order_relaxed);
//        tail_.store(o.tail(), std::memory_order_relaxed);
//        o.head_.store(nullptr, std::memory_order_relaxed);
//        o.tail_.store(nullptr, std::memory_order_relaxed);
//        return *this;
//    }
//
//    ~shared_head_tail_list()
//    {
//        DCHECK(head() == nullptr);
//        DCHECK(tail() == nullptr);
//    }
//
//    void push(Node *node) noexcept
//    {
//        bool done = false;
//        while (!done) {
//            if (tail()) {
//                done = push_in_non_empty_list(node);
//            }
//            else {
//                done = push_in_empty_list(node);
//            }
//        }
//    }
//
//    bool empty() const noexcept
//    {
//        return head() == nullptr;
//    }
//
//private:
//    Node *head() const noexcept
//    {
//        return head_.load(std::memory_order_acquire);
//    }
//
//    Node *tail() const noexcept
//    {
//        return tail_.load(std::memory_order_acquire);
//    }
//
//    void set_head(Node *node) noexcept
//    {
//        head_.store(node, std::memory_order_release);
//    }
//
//    bool cas_head(Node *expected, Node *node) noexcept
//    {
//        return head_.compare_exchange_weak(
//            expected, node, std::memory_order_acq_rel, std::memory_order_relaxed);
//    }
//
//    bool cas_tail(Node *expected, Node *node) noexcept
//    {
//        return tail_.compare_exchange_weak(
//            expected, node, std::memory_order_acq_rel, std::memory_order_relaxed);
//    }
//
//    Node *exchange_head() noexcept
//    {
//        return head_.exchange(nullptr, std::memory_order_acq_rel);
//    }
//
//    Node *exchange_tail() noexcept
//    {
//        return tail_.exchange(nullptr, std::memory_order_acq_rel);
//    }
//}; // shared_head_tail_list

// Reference-counted object base class
template<typename T, typename Deleter = default_deleter<T>>
struct ref_count_base
{
    std::atomic<std::size_t> ref_count;

    // By default the reference count is initialized to 1
    explicit ref_count_base(std::size_t count = 1)
        : ref_count(count)
    {}

    void add_ref(std::size_t count = 1)
    {
        ref_count.fetch_add(count, std::memory_order_relaxed);
    }
    void remove_ref(std::size_t count = 1)
    {
        if (ref_count.fetch_sub(count, std::memory_order_release) == count) {
            std::atomic_thread_fence(std::memory_order_acquire);
            Deleter::do_delete(static_cast<T *>(this));
        }
    }
    void add_ref_unlocked()
    {
        ref_count.store(ref_count.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    }
    bool is_unique_ref(std::memory_order order)
    {
        return ref_count.load(order) == 1;
    }
};

// Pointer to reference counted object, based on boost::intrusive_ptr
template<typename T>
class ref_count_ptr
{
    T *p;

public:
    // Note that this doesn't increment the reference count, instead it takes
    // ownership of a pointer which you already own a reference to.
    explicit ref_count_ptr(T *t)
        : p(t)
    {}

    ref_count_ptr()
        : p(nullptr)
    {}
    ref_count_ptr(std::nullptr_t)
        : p(nullptr)
    {}
    ref_count_ptr(const ref_count_ptr &other) LIBASYNC_NOEXCEPT
        : p(other.p)
    {
        if (p)
            p->add_ref();
    }
    ref_count_ptr(ref_count_ptr &&other) LIBASYNC_NOEXCEPT
        : p(other.p)
    {
        other.p = nullptr;
    }
    ref_count_ptr &operator=(std::nullptr_t)
    {
        if (p)
            p->remove_ref();
        p = nullptr;
        return *this;
    }
    ref_count_ptr &operator=(const ref_count_ptr &other) LIBASYNC_NOEXCEPT
    {
        if (p) {
            p->remove_ref();
            p = nullptr;
        }
        p = other.p;
        if (p)
            p->add_ref();
        return *this;
    }
    ref_count_ptr &operator=(ref_count_ptr &&other) LIBASYNC_NOEXCEPT
    {
        if (p) {
            p->remove_ref();
            p = nullptr;
        }
        p = other.p;
        other.p = nullptr;
        return *this;
    }
    ~ref_count_ptr()
    {
        if (p)
            p->remove_ref();
    }

    T &operator*() const
    {
        return *p;
    }
    T *operator->() const
    {
        return p;
    }
    T *get() const
    {
        return p;
    }
    T *release()
    {
        T *out = p;
        p = nullptr;
        return out;
    }

    explicit operator bool() const
    {
        return p != nullptr;
    }
    friend bool operator==(const ref_count_ptr &a, const ref_count_ptr &b)
    {
        return a.p == b.p;
    }
    friend bool operator!=(const ref_count_ptr &a, const ref_count_ptr &b)
    {
        return a.p != b.p;
    }
    friend bool operator==(const ref_count_ptr &a, std::nullptr_t)
    {
        return a.p == nullptr;
    }
    friend bool operator!=(const ref_count_ptr &a, std::nullptr_t)
    {
        return a.p != nullptr;
    }
    friend bool operator==(std::nullptr_t, const ref_count_ptr &a)
    {
        return a.p == nullptr;
    }
    friend bool operator!=(std::nullptr_t, const ref_count_ptr &a)
    {
        return a.p != nullptr;
    }
};

} // namespace detail
} // namespace stagefuture

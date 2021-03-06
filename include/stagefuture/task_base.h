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

// Task states
enum class task_state: unsigned char
{
    pending, // Task has not completed yet
    locked, // Task is locked (used by event_event to prevent double set)
    unwrapped, // Task is waiting for an unwrapped task to finish
    completed, // Task has finished execution and a result is available
    canceled // Task has been canceled and an exception is available
};

// Determine whether a task is in a final state
inline bool is_finished(task_state s)
{
    return s == task_state::completed || s == task_state::canceled;
}

class task_interface
{
public:
    // Run the associated function
    virtual void run(task_ptr t) LIBASYNC_NOEXCEPT = 0;

    // Cancel the task with an exception
    virtual void cancel(task_ptr t, std::exception_ptr &&) LIBASYNC_NOEXCEPT = 0;

    // Schedule the task using its scheduler
    virtual void schedule(task_ptr parent, task_ptr t) = 0;
};

// Type-generic base task object
struct LIBASYNC_CACHELINE_ALIGN task_base: public task_interface,
                                           std::enable_shared_from_this<task_base>
{
// Task state
    std::atomic<task_state> state;

// Whether get_task() was already called on an event_event
    bool event_task_got_task;

// Vector of continuations
    continuation_vector continuations;

// Use aligned memory allocation
    void *operator new(std::size_t size)
    {
        return aligned_alloc(size, LIBASYNC_CACHELINE_SIZE);
    }
    void operator delete(void *ptr)
    {
        aligned_free(ptr);
    }

// Initialize task state
    task_base()
        : state(task_state::pending)
    {}

// Check whether the task is ready and include an acquire barrier if it is
    bool ready() const
    {
        return is_finished(state.load(std::memory_order_acquire));
    }

// Run a single continuation
    void run_continuation(detail::scheduler &sched, task_ptr &&cont)
    {
        LIBASYNC_TRY {
            detail::schedule_task(sched, std::move(cont));
        } LIBASYNC_CATCH(...) {
            // This is suboptimal, but better than letting the exception leak
            cont->cancel(cont, std::current_exception());
        }
    }

// Run all of the task's continuations after it has completed or canceled.
// The list of continuations is emptied and locked to prevent any further
// continuations from being added.
    void run_continuations()
    {
        continuations.flush_and_lock([this](task_ptr t)
                                     {
                                         t->schedule(shared_from_this(), std::move(t));
                                     });
    }

// Add a continuation to this task
    void add_continuation(detail::scheduler &sched, task_ptr cont)
    {
        // Check for task completion
        task_state current_state = state.load(std::memory_order_relaxed);
        if (!is_finished(current_state)) {
            // Try to add the task to the continuation list. This can fail only
            // if the task has just finished, in which case we run it directly.
            if (continuations.try_add(std::move(cont)))
                return;
        }

        // Otherwise run the continuation directly
        std::atomic_thread_fence(std::memory_order_acquire);
        run_continuation(sched, std::move(cont));
    }

// Finish the task after it has been executed and the result set
    void finish()
    {
        state.store(task_state::completed, std::memory_order_release);
        run_continuations();
    }

// Wait for the task to finish executing
    task_state wait()
    {
        task_state s = state.load(std::memory_order_acquire);
        if (!is_finished(s)) {
            wait_for_task(shared_from_this());
            s = state.load(std::memory_order_relaxed);
        }
        return s;
    }
};

// Result type-specific task object
template<typename Result>
struct task_result_holder: public task_base
{
    struct
    {
        typename std::aligned_storage<sizeof(Result), std::alignment_of<Result>::value>::type result;
        std::aligned_storage<sizeof(std::exception_ptr), std::alignment_of<std::exception_ptr>::value>::type except;

        // Scheduler that should be used to schedule this task. The scheduler
        // type has been erased and is held by vtable->schedule.
        detail::scheduler *sched;
    };

    template<typename T>
    void set_result(T &&t)
    {
        new(&result) Result(std::forward<T>(t));
    }

    // Return a result using an lvalue or rvalue reference depending on the task
    // type. The task parameter is not used, it is just there for overload resolution.
    template<typename T>
    Result &&get_result(const stage_future<T> &)
    {
        return std::move(*reinterpret_cast<Result *>(&result));
    }
    template<typename T>
    const Result &get_result(const shared_stage_future<T> &)
    {
        return *reinterpret_cast<Result *>(&result);
    }

    // Destroy the result
    ~task_result_holder()
    {
        // Result is only present if the task completed successfully
        if (state.load(std::memory_order_relaxed) == task_state::completed)
            reinterpret_cast<Result *>(&result)->~Result();
    }
};

// Specialization for references
template<typename Result>
struct task_result_holder<Result &>: public task_base
{
    struct
    {
        // Store as pointer internally
        Result *result;
        std::aligned_storage<sizeof(std::exception_ptr), std::alignment_of<std::exception_ptr>::value>::type except;
        detail::scheduler *sched;
    };

    void set_result(Result &obj)
    {
        result = std::addressof(obj);
    }

    template<typename T>
    Result &get_result(const stage_future<T> &)
    {
        return *result;
    }
    template<typename T>
    Result &get_result(const shared_stage_future<T> &)
    {
        return *result;
    }
};

// Specialization for void
template<>
struct task_result_holder<fake_void>: public task_base
{
    struct
    {
        std::aligned_storage<sizeof(std::exception_ptr), std::alignment_of<std::exception_ptr>::value>::type except;
        detail::scheduler *sched;
    };

    void set_result(fake_void)
    {}

    // Get the result as fake_void so that it can be passed to set_result and
    // continuations
    template<typename T>
    fake_void get_result(const stage_future<T> &)
    {
        return fake_void();
    }
    template<typename T>
    fake_void get_result(const shared_stage_future<T> &)
    {
        return fake_void();
    }
};

template<typename Result>
struct task_result: public task_result_holder<Result>
{
    task_result()
    {
    }

    // Destroy the exception
    ~task_result()
    {
        // Exception is only present if the task was canceled
        if (this->state.load(std::memory_order_relaxed) == task_state::canceled)
            reinterpret_cast<std::exception_ptr *>(&this->except)->~exception_ptr();
    }

    // Cancel a task with the given exception
    void cancel_base(std::exception_ptr &&except)
    {
        set_exception(std::move(except));
        this->state.store(task_state::canceled, std::memory_order_release);
        this->run_continuations();
    }

    // Set the exception value of the task
    void set_exception(std::exception_ptr &&except)
    {
        new(&this->except) std::exception_ptr(std::move(except));
    }

    // Get the exception a task was canceled with
    std::exception_ptr &get_exception()
    {
        return *reinterpret_cast<std::exception_ptr *>(&this->except);
    }

    // Wait and throw the exception if the task was canceled
    void wait_and_throw()
    {
        if (this->wait() == task_state::canceled)
            LIBASYNC_RETHROW_EXCEPTION(get_exception());
    }

    void run(task_ptr t) LIBASYNC_NOEXCEPT override
    {

    }

    void cancel(task_ptr t, std::exception_ptr &&ptr) LIBASYNC_NOEXCEPT override
    {

    }

    void schedule(task_ptr parent, task_ptr t) override
    {

    }
};

// Class to hold a function object, with empty base class optimization
template<typename Func, typename = void>
struct func_base
{
    Func func;

    template<typename F>
    explicit func_base(F &&f)
        : func(std::forward<F>(f))
    {}
    Func &get_func()
    {
        return func;
    }
};
template<typename Func>
struct func_base<Func, typename std::enable_if<std::is_empty<Func>::value>::type>
{
    template<typename F>
    explicit func_base(F &&f)
    {
        new(this) Func(std::forward<F>(f));
    }
    ~func_base()
    {
        get_func().~Func();
    }
    Func &get_func()
    {
        return *reinterpret_cast<Func *>(this);
    }
};

// Class to hold a function object and initialize/destroy it at any time
template<typename Func, typename = void>
struct func_holder
{
    typename std::aligned_storage<sizeof(Func), std::alignment_of<Func>::value>::type func;

    Func &get_func()
    {
        return *reinterpret_cast<Func *>(&func);
    }
    template<typename... Args>
    void init_func(Args &&... args)
    {
        new(&func) Func(std::forward<Args>(args)...);
    }
    void destroy_func()
    {
        get_func().~Func();
    }
};
template<typename Func>
struct func_holder<Func, typename std::enable_if<std::is_empty<Func>::value>::type>
{
    Func &get_func()
    {
        return *reinterpret_cast<Func *>(this);
    }
    template<typename... Args>
    void init_func(Args &&... args)
    {
        new(this) Func(std::forward<Args>(args)...);
    }
    void destroy_func()
    {
        get_func().~Func();
    }
};

// Task object with an associated function object
// Using private inheritance so empty Func doesn't take up space
template<typename Func, typename Result>
struct task_func: public task_result<Result>, func_holder<Func>
{
    template<typename... Args>
    explicit task_func(Args &&... args)
    {
        this->init_func(std::forward<Args>(args)...);
    }

    // Free the function
    ~task_func()
    {
        // If the task hasn't completed yet, destroy the function object. Note
        // that an unwrapped task has already destroyed its function object.
        if (this->state.load(std::memory_order_relaxed) == task_state::pending)
            this->destroy_func();
    }

    void run(task_ptr t) LIBASYNC_NOEXCEPT override
    {
        LIBASYNC_TRY {
            // Dispatch to execution function
            std::static_pointer_cast<task_func<Func, Result>>(t)->get_func()(t);
        } LIBASYNC_CATCH(...) {
            cancel(t, std::current_exception());
        }
    }

    void cancel(task_ptr t, std::exception_ptr &&except) LIBASYNC_NOEXCEPT override
    {
        // Destroy the function object when canceling since it won't be
        // used anymore.
        std::static_pointer_cast<task_func<Func, Result>>(t)->destroy_func();
        std::static_pointer_cast<task_func<Func, Result>>(t)->cancel_base(std::move(except));
    }

    void schedule(task_ptr parent, task_ptr t) override
    {
        parent->run_continuation((*(this->sched)), std::move(t));
    }
};

// Helper functions to access the internal_task member of a task object, which
// avoids us having to specify half of the functions in the detail namespace
// as friend. Also, internal_task is downcast to the appropriate task_result<>.
template<typename Task>
typename Task::internal_task_type get_internal_task(const Task &t)
{
    return t.get_internal_task();
}

template<typename Task>
void set_internal_task(Task &t, task_ptr p)
{
    t.set_internal_task(std::move(p));
}

// Common code for task unwrapping
template<typename Result, typename Child>
struct unwrapped_func
{
    explicit unwrapped_func(task_ptr t)
        : parent_task(std::move(t))
    {}
    void operator()(Child child_task) const
    {
        // Forward completion state and result to parent task
        std::shared_ptr<task_result<Result>> parent = std::static_pointer_cast<task_result<Result>>(parent_task);
        LIBASYNC_TRY {
            if (get_internal_task(child_task)->state.load(std::memory_order_relaxed) == task_state::completed) {
                parent->set_result(get_internal_task(child_task)->get_result(child_task));
                parent->finish();
            }
            else {
                // We don't call the generic cancel function here because
                // the function of the parent task has already been destroyed.
                parent->cancel_base(std::exception_ptr(get_internal_task(child_task)->get_exception()));
            }
        } LIBASYNC_CATCH(...) {
            // If the copy/move constructor of the result threw, propagate the exception
            parent->cancel_base(std::current_exception());
        }
    }
    task_ptr parent_task;
};
template<typename Result, typename Func, typename Child>
void unwrapped_finish(detail::task_ptr parent_base, Child child_task)
{
    // Destroy the parent task's function since it has been executed
    parent_base->state.store(task_state::unwrapped, std::memory_order_relaxed);
    std::shared_ptr<task_func<Func, Result>>
        pParentFunc = std::static_pointer_cast<task_func<Func, Result> >(parent_base);
    pParentFunc->destroy_func();

    // Set up a continuation on the child to set the result of the parent
    LIBASYNC_TRY {
        child_task.then(*(pParentFunc->sched), unwrapped_func<Result, Child>(task_ptr(parent_base)));
    } LIBASYNC_CATCH(...) {
        // Use cancel_base here because the function object is already destroyed.
        std::static_pointer_cast<task_result<Result> >(parent_base)->cancel_base(std::current_exception());
    }
}

// Execution functions for root tasks:
// - With and without task unwraping
template<typename Result, typename Func, bool Unwrap>
struct root_exec_func: private func_base<Func>
{
    template<typename F>
    explicit root_exec_func(F &&f)
        : func_base<Func>(std::forward<F>(f))
    {}
    void operator()(detail::task_ptr t)
    {
        std::static_pointer_cast<task_result<Result> >(t)
            ->set_result(detail::invoke_fake_void(std::move(this->get_func())));
        std::static_pointer_cast<task_func<root_exec_func, Result> >(t)->destroy_func();
        t->finish();
    }
};
template<typename Result, typename Func>
struct root_exec_func<Result, Func, true>: private func_base<Func>
{
    template<typename F>
    explicit root_exec_func(F &&f)
        : func_base<Func>(std::forward<F>(f))
    {}
    void operator()(detail::task_ptr t)
    {
        stage_future<Result> resFuture = std::move(this->get_func())();
        unwrapped_finish<Result, root_exec_func>(t, std::move(resFuture));
    }
};

// Execution functions for continuation tasks:
// - With and without task unwraping
// - For value-based and task-based continuations
template<typename Parent, typename Result, typename Func, bool ValueCont, bool Unwrap>
struct continuation_exec_func: private func_base<Func>
{
    template<typename F, typename P>
    continuation_exec_func(F &&f, P &&p)
        : func_base<Func>(std::forward<F>(f)), parent(std::forward<P>(p))
    {}
    void operator()(detail::task_ptr t)
    {
        std::static_pointer_cast<task_result<Result>>(t)
            ->set_result(detail::invoke_fake_void(std::move(this->get_func()), std::move(parent)));
        std::static_pointer_cast<task_func<continuation_exec_func, Result> >(t)->destroy_func();
        t->finish();
    }
    Parent parent;
};
template<typename Parent, typename Result, typename Func>
struct continuation_exec_func<Parent, Result, Func, true, false>: private func_base<Func>
{
    template<typename F, typename P>
    continuation_exec_func(F &&f, P &&p)
        : func_base<Func>(std::forward<F>(f)), parent(std::forward<P>(p))
    {}
    void operator()(detail::task_ptr t)
    {
        if (get_internal_task(parent)->state.load(std::memory_order_relaxed) == task_state::canceled)
            t->cancel(t, std::exception_ptr(get_internal_task(parent)->get_exception()));
        else {
            std::static_pointer_cast<task_result<Result> >(t)
                ->set_result(detail::invoke_fake_void(std::move(this->get_func()),
                                                      get_internal_task(parent)
                                                          ->get_result(parent)));
            std::static_pointer_cast<task_func<continuation_exec_func, Result>>(t)->destroy_func();
            t->finish();
        }
    }
    Parent parent;
};
template<typename Parent, typename Result, typename Func>
struct continuation_exec_func<Parent, Result, Func, false, true>: private func_base<Func>
{
    template<typename F, typename P>
    continuation_exec_func(F &&f, P &&p)
        : func_base<Func>(std::forward<F>(f)), parent(std::forward<P>(p))
    {}
    void operator()(detail::task_ptr t)
    {
        unwrapped_finish<Result, continuation_exec_func>(t,
                                                         detail::invoke_fake_void(std::move(this->get_func()),
                                                                                  std::move(parent)));
    }
    Parent parent;
};
template<typename Parent, typename Result, typename Func>
struct continuation_exec_func<Parent, Result, Func, true, true>: private func_base<Func>
{
    template<typename F, typename P>
    continuation_exec_func(F &&f, P &&p)
        : func_base<Func>(std::forward<F>(f)), parent(std::forward<P>(p))
    {}
    void operator()(detail::task_ptr t)
    {
        if (get_internal_task(parent)->state.load(std::memory_order_relaxed) == task_state::canceled)
            t->cancel(t, std::exception_ptr(get_internal_task(parent)->get_exception()));
        else
            unwrapped_finish<Result, continuation_exec_func>(t,
                                                             detail::invoke_fake_void(std::move(this->get_func()),
                                                                                      get_internal_task(parent)
                                                                                          ->get_result(parent)));
    }
    Parent parent;
};

template<typename Res, typename Par, bool ParVoid>
struct future_func_type
{
    typedef std::function<Res(Par)> type;
public:
    static std::function<Res(Par)> composeCall(type &&func)
    {
        return [&func](Par par) -> Res
        {
            return std::move(func(par));
        };
    }
};

template<typename Res, typename Par>
struct future_func_type<Res, Par, true>
{
    typedef std::function<Res()> type;
public:
    static std::function<Res()> composeCall(type &&func)
    {
        return [&func]() -> Res
        {
            return std::move(func());
        };
    }
};

// Create a canceled task containing an exception
template<typename T>
stage_future<T> combine_canceled(std::exception_ptr except)
{
    stage_future<T> out;

    detail::set_internal_task(out,
                              detail::task_ptr(new detail::task_result<typename detail::void_to_fake_void<T>::type>));
    detail::get_internal_task(out)->set_exception(std::move(except));
    detail::get_internal_task(out)->state.store(detail::task_state::canceled, std::memory_order_relaxed);

    return out;
}

template<typename Res, typename Par1, typename Par2, bool Par1Void, bool Par2Void>
struct future_2func_type
{
    typedef std::function<Res(Par1, Par2)> type;
public:
    typedef std::tuple<stagefuture::stage_future<Par1>,
                       stagefuture::stage_future<typename detail::remove_task<Par2>::type>> par_type;
    static std::function<Res(par_type)> combineCall(type &&func)
    {
        return [&func](par_type results) -> Res
        {
            auto&& res0 = std::get<0>(results);
            auto&& res1 = std::get<1>(results);
            if (res0.canceled()) {
                throw res0.get_exception();
            }
            if (res1.canceled()) {
                throw res1.get_exception();
            }
            Par1 par1 = res0.get();
            Par2 par2 = res1.get();
            return func(par1, par2);
        };
    }
};

template<typename Res, typename Par1, typename Par2>
struct future_2func_type<Res, Par1, Par2, true, true>
{
    typedef std::function<Res()> type;

public:
    typedef std::tuple<stagefuture::stage_future<Par1>,
                       stagefuture::stage_future<typename detail::remove_task<Par2>::type>> par_type;
    static std::function<Res(par_type)> combineCall(type &&func)
    {
        return [&func](par_type results) -> Res
        {
            auto&& res0 = std::get<0>(results);
            auto&& res1 = std::get<1>(results);
            if (res0.canceled()) {
                throw res0.get_exception();
            }
            if (res1.canceled()) {
                throw res1.get_exception();
            }
            return func();
        };
    }
};

template<typename Res, typename Par1, typename Par2>
struct future_2func_type<Res, Par1, Par2, true, false>
{
    typedef std::function<Res(Par2)> type;
public:
    typedef std::tuple<stagefuture::stage_future<Par1>,
                       stagefuture::stage_future<typename detail::remove_task<Par2>::type>> par_type;
    static std::function<Res(par_type)> combineCall(type &&func)
    {
        return [&func](par_type results) -> Res
        {
            auto&& res0 = std::get<0>(results);
            auto&& res1 = std::get<1>(results);
            if (res0.canceled()) {
                throw res0.get_exception();
            }
            if (res1.canceled()) {
                throw res1.get_exception();
            }
            Par2 par2 = res1.get();
            return func(par2);
        };
    }
};

template<typename Res, typename Par1, typename Par2>
struct future_2func_type<Res, Par1, Par2, false, true>
{
    typedef std::function<Res(Par1)> type;
public:
    typedef std::tuple<stagefuture::stage_future<Par1>,
                       stagefuture::stage_future<typename detail::remove_task<Par2>::type>> par_type;
    static std::function<Res(par_type)> combineCall(type &&func)
    {
        return [&func](par_type results) -> Res
        {
            auto&& res0 = std::get<0>(results);
            auto&& res1 = std::get<1>(results);
            if (res0.canceled()) {
                throw res0.get_exception();
            }
            if (res1.canceled()) {
                throw res1.get_exception();
            }
            Par1 par1 = res0.get();
            return func(par1);
        };
    }
};

} // namespace detail
} // namespace stagefuture

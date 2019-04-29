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

// Exception thrown when an event_event is destroyed without setting a value
struct LIBASYNC_EXPORT_EXCEPTION abandoned_event_task
{
};

namespace detail
{

// Common code for task and shared_stage_future
template<typename Result>
class basic_future
{
private:
    // Reference counted internal task object
    detail::task_ptr internal_task;

public:
    // Real result type, with void turned into fake_void
    typedef typename void_to_fake_void<Result>::type internal_result;

    // Type-specific task object
    typedef task_result<internal_result> task_type;
    typedef std::shared_ptr<task_type> internal_task_type;
    // Friend access
    friend stagefuture::stage_future<Result>;
    friend stagefuture::shared_stage_future<Result>;

    internal_task_type get_internal_task() const
    {
        return std::static_pointer_cast<task_type>(internal_task);
    }

    void set_internal_task(task_ptr p)
    {
        internal_task = p;
    }

    // Common code for get()
    void get_internal() const
    {
        LIBASYNC_ASSERT(internal_task, std::invalid_argument, "Use of empty task object");

        // If the task was canceled, throw the associated exception
        get_internal_task()->wait_and_throw();
    }

    // Common code for then()
    template<typename Func, typename Parent>
    typename continuation_traits<Parent, Func>::future_type
    then_internal(detail::scheduler &sched, Func &&f, Parent &&parent) const
    {
        LIBASYNC_ASSERT(internal_task, std::invalid_argument, "Use of empty task object");

        // Save a copy of internal_task because it might get moved into exec_func
        task_base *my_internal = internal_task.get();

        // Create continuation
        typedef continuation_traits<Parent, Func> traits;
        typedef typename void_to_fake_void<typename traits::future_type::result_type>::type cont_internal_result;
        typedef continuation_exec_func<typename std::decay<Parent>::type,
                                       cont_internal_result,
                                       typename traits::decay_func,
                                       traits::is_value_cont::value,
                                       is_stage_future<typename traits::result_type>::value> exec_func;
        typename traits::future_type cont;
        task_ptr taskPtr = task_ptr(new task_func<exec_func, cont_internal_result>(std::forward<Func>(f),
                                                                                   std::forward<Parent>(parent)));
        cont.set_internal_task(taskPtr);

        cont.get_internal_task()->sched = std::addressof(sched);
        // Add the continuation to this task
        my_internal->add_continuation(sched, taskPtr);
        return cont;
    }

public:
    // Task result type
    typedef Result result_type;

    // Check if this task is not empty
    bool valid() const
    {
        return internal_task != nullptr;
    }

    // Query whether the task has finished executing
    bool ready() const
    {
        LIBASYNC_ASSERT(internal_task, std::invalid_argument, "Use of empty task object");
        return internal_task->ready();
    }

    // Query whether the task has been canceled with an exception
    bool canceled() const
    {
        LIBASYNC_ASSERT(internal_task, std::invalid_argument, "Use of empty task object");
        return internal_task->state.load(std::memory_order_acquire) == task_state::canceled;
    }

    // Wait for the task to complete
    void wait() const
    {
        LIBASYNC_ASSERT(internal_task, std::invalid_argument, "Use of empty task object");
        internal_task->wait();
    }

    // Get the exception associated with a canceled task
    std::exception_ptr get_exception() const
    {
        LIBASYNC_ASSERT(internal_task, std::invalid_argument, "Use of empty task object");
        if (internal_task->wait() == task_state::canceled)
            return get_internal_task(*this)->get_exception();
        else
            return std::exception_ptr();
    }
};

// Common code for event_event specializations
template<typename Result>
class basic_event
{
    // Reference counted internal task object
    detail::task_ptr internal_task;

    // Real result type, with void turned into fake_void
    typedef typename detail::void_to_fake_void<Result>::type internal_result;
    typedef task_result<internal_result> task_type;

    // Type-specific task object
    typedef std::shared_ptr<task_type> internal_task_type;

    // Friend access
    friend stagefuture::event_event<Result>;

    internal_task_type get_internal_task() const
    {
        return std::static_pointer_cast<task_type>(internal_task);
    }

    void set_internal_task(task_ptr p)
    {
        internal_task = p;
    }

    // Common code for set()
    template<typename T>
    bool set_internal(T &&result) const
    {
        LIBASYNC_ASSERT(internal_task, std::invalid_argument, "Use of empty event_event object");

        // Only allow setting the value once
        detail::task_state expected = detail::task_state::pending;
        if (!internal_task->state.compare_exchange_strong(expected,
                                                          detail::task_state::locked,
                                                          std::memory_order_acquire,
                                                          std::memory_order_relaxed))
            return false;

        LIBASYNC_TRY {
            // Store the result and finish
            get_internal_task()->set_result(std::forward<T>(result));
            internal_task->finish();
        } LIBASYNC_CATCH(...) {
            // At this point we have already committed to setting a value, so
            // we can't return the exception to the caller. If we did then it
            // could cause concurrent set() calls to fail, thinking a value has
            // already been set. Instead, we simply cancel the task with the
            // exception we just got.
            get_internal_task()->cancel_base(std::current_exception());
        }
        return true;
    }

public:
    // Movable but not copyable
    basic_event(basic_event &&other) LIBASYNC_NOEXCEPT
        : internal_task(std::move(other.internal_task))
    {}
    basic_event &operator=(basic_event &&other) LIBASYNC_NOEXCEPT
    {
        internal_task = std::move(other.internal_task);
        return *this;
    }

    // Main constructor
    basic_event()
        : internal_task(new internal_task_type)
    {
        internal_task->event_task_got_task = false;
    }

    // Cancel events if they are destroyed before they are set
    ~basic_event()
    {
        // This check isn't thread-safe but set_exception does a proper check
        if (internal_task && !internal_task->ready() && !internal_task.use_count() == 1) {
#ifdef LIBASYNC_NO_EXCEPTIONS
            // This will result in an abort if the task result is read
            set_exception(std::exception_ptr());
#else
            set_exception(std::make_exception_ptr(abandoned_event_task()));
#endif
        }
    }

    // Get the task linked to this event. This can only be called once.
    stage_future<Result> get_task()
    {
        LIBASYNC_ASSERT(internal_task, std::invalid_argument, "Use of empty event_event object");
        LIBASYNC_ASSERT(!internal_task->event_task_got_task,
                        std::logic_error,
                        "get_task() called twice on event_event");

        // Even if we didn't trigger an assert, don't return a task if one has
        // already been returned.
        stage_future<Result> out;
        if (!internal_task->event_task_got_task)
            out.set_internal_task(internal_task);
        internal_task->event_task_got_task = true;
        return out;
    }

    // Cancel the event with an exception and cancel continuations
    bool set_exception(std::exception_ptr except) const
    {
        LIBASYNC_ASSERT(internal_task, std::invalid_argument, "Use of empty event_event object");

        // Only allow setting the value once
        detail::task_state expected = detail::task_state::pending;
        if (!internal_task->state.compare_exchange_strong(expected,
                                                          detail::task_state::locked,
                                                          std::memory_order_acquire,
                                                          std::memory_order_relaxed))
            return false;

        // Cancel the task
        get_internal_task()->cancel_base(std::move(except));
        return true;
    }
};

} // namespace detail

template<typename Result>
class stage_future: public detail::basic_future<Result>
{
public:
    // Movable but not copyable
    stage_future() = default;
    stage_future(stage_future &&other) LIBASYNC_NOEXCEPT
        : detail::basic_future<Result>(std::move(other))
    {}
    stage_future &operator=(stage_future &&other) LIBASYNC_NOEXCEPT
    {
        detail::basic_future<Result>::operator=(std::move(other));
        return *this;
    }

    // Get the result of the task
    Result get()
    {
        this->get_internal();

        // Move the internal state pointer so that the task becomes invalid,
        // even if an exception is thrown.
        detail::task_ptr my_internal = std::move(this->internal_task);
        return detail::fake_void_to_void(static_cast<typename stage_future::task_type *>(my_internal.get())
                                             ->get_result(*this));
    }

    // Add a continuation to the task
    template<typename Sched, typename Func>
    typename detail::continuation_traits<stage_future, Func>::future_type then(Sched &sched, Func &&f)
    {
        return this->then_internal(sched, std::forward<Func>(f), std::move(*this));
    }

    template<typename Func>
    typename detail::continuation_traits<stage_future, Func>::future_type then(Func &&f)
    {
        return then(::stagefuture::default_scheduler(), std::forward<Func>(f));
    }

    template<typename Func>
    typename detail::continuation_traits<stage_future, Func>::future_type thenApply(Func &&f)
    {
        typedef typename detail::continuation_traits<stage_future, Func>::result_type return_type;
        //the type of the function's return value must not be void
        static_assert(!std::is_void<return_type>::value, "The type of the func's result is must not be void");
        detail::scheduler *pScheduler = get_internal_task(*this)->sched;
        //如果父亲任务的sched为null则
        if (pScheduler == nullptr) {
            pScheduler = &inline_scheduler();
        }
        return this->then_internal(*pScheduler, std::forward<Func>(f), std::move(*this));
    }

    // Add a continuation to the task
    template<typename Func>
    typename detail::continuation_traits<stage_future, Func>::future_type
    thenApplyAsync(detail::scheduler &sched, Func &&f)
    {
        typedef typename detail::continuation_traits<stage_future, Func>::result_type return_type;
        //the type of the function's return value must not be void
        static_assert(!std::is_void<return_type>::value, "The type of the func's result is must not be void");
        return this->then_internal(sched, std::forward<Func>(f), std::move(*this));
    }

    template<typename Func>
    typename detail::continuation_traits<stage_future, Func>::future_type thenApplyAsync(Func &&f)
    {
        typedef typename detail::continuation_traits<stage_future, Func>::result_type return_type;
        //the type of the function's return value must not be void
        static_assert(!std::is_void<return_type>::value, "The type of the func's result is must not be void");
        return this->then_internal(::stagefuture::default_scheduler(), std::forward<Func>(f), std::move(*this));
    }

    template<typename Func>
    stage_future<void> thenAccept(Func &&f)
    {
        typedef typename detail::continuation_traits<stage_future, Func>::result_type return_type;
        //the type of the function's return value must not be void
        static_assert(std::is_void<return_type>::value, "The type of the func's result is must be void");
        detail::scheduler *pScheduler = get_internal_task(*this)->sched;
        //如果父亲任务的sched为null则
        if (pScheduler == nullptr) {
            pScheduler = &inline_scheduler();
        }
        return this->then_internal(*pScheduler, std::forward<Func>(f), std::move(*this));
    }

    template<typename Func>
    stage_future<void> thenAcceptAsync(detail::scheduler &sched, Func &&f)
    {
        typedef typename detail::continuation_traits<stage_future, Func>::result_type return_type;
        //the type of the function's return value must not be void
        static_assert(std::is_void<return_type>::value, "The type of the func's result is must be void");
        return this->then_internal(sched, std::forward<Func>(f), std::move(*this));
    }

    template<typename Func>
    stage_future<void> thenAcceptAsync(Func &&f)
    {
        typedef typename detail::continuation_traits<stage_future, Func>::result_type return_type;
        //the type of the function's return value must not be void
        static_assert(std::is_void<return_type>::value, "The type of the func's result is must be void");
        return this->then_internal(::stagefuture::default_scheduler(), std::forward<Func>(f), std::move(*this));
    }

    // Create a shared_stage_future from this task
    shared_stage_future<Result> share()
    {
        LIBASYNC_ASSERT(this->internal_task, std::invalid_argument, "Use of empty task object");

        shared_stage_future<Result> out;
        detail::set_internal_task(out, std::move(this->internal_task));
        return out;
    }
};

template<typename Result>
class shared_stage_future: public detail::basic_future<Result>
{
    // get() return value: const Result& -or- void
    typedef typename std::conditional<
        std::is_void<Result>::value,
        void,
        typename std::add_lvalue_reference<
            typename std::add_const<Result>::type
        >::type
    >::type get_result;

public:
    // Movable and copyable
    shared_stage_future() = default;

    // Get the result of the task
    get_result get() const
    {
        this->get_internal();
        return detail::fake_void_to_void(detail::get_internal_task(*this)->get_result(*this));
    }

    // Add a continuation to the task
    template<typename Sched, typename Func>
    typename detail::continuation_traits<shared_stage_future, Func>::task_type then(Sched &sched, Func &&f) const
    {
        return this->then_internal(sched, std::forward<Func>(f), *this);
    }
    template<typename Func>
    typename detail::continuation_traits<shared_stage_future, Func>::task_type then(Func &&f) const
    {
        return then(::stagefuture::default_scheduler(), std::forward<Func>(f));
    }
};

// Special task type which can be triggered manually rather than when a function executes.
template<typename Result>
class event_event: public detail::basic_event<Result>
{
public:
    // Movable but not copyable
    event_event() = default;
    event_event(event_event &&other) LIBASYNC_NOEXCEPT
        : detail::basic_event<Result>(std::move(other))
    {}
    event_event &operator=(event_event &&other) LIBASYNC_NOEXCEPT
    {
        detail::basic_event<Result>::operator=(std::move(other));
        return *this;
    }

    // Set the result of the task, mark it as completed and run its continuations
    bool set(const Result &result) const
    {
        return this->set_internal(result);
    }
    bool set(Result &&result) const
    {
        return this->set_internal(std::move(result));
    }
};

// Specialization for references
template<typename Result>
class event_event<Result &>: public detail::basic_event<Result &>
{
public:
    // Movable but not copyable
    event_event() = default;
    event_event(event_event &&other) LIBASYNC_NOEXCEPT
        : detail::basic_event<Result &>(std::move(other))
    {}
    event_event &operator=(event_event &&other) LIBASYNC_NOEXCEPT
    {
        detail::basic_event<Result &>::operator=(std::move(other));
        return *this;
    }

    // Set the result of the task, mark it as completed and run its continuations
    bool set(Result &result) const
    {
        return this->set_internal(result);
    }
};

// Specialization for void
template<>
class event_event<void>: public detail::basic_event<void>
{
public:
    // Movable but not copyable
    event_event() = default;
    event_event(event_event &&other) LIBASYNC_NOEXCEPT
        : detail::basic_event<void>(std::move(other))
    {}
    event_event &operator=(event_event &&other) LIBASYNC_NOEXCEPT
    {
        detail::basic_event<void>::operator=(std::move(other));
        return *this;
    }

    // Set the result of the task, mark it as completed and run its continuations
    bool set()
    {
        return this->set_internal(detail::fake_void());
    }
};

// Task type returned by local_spawn()
template<typename Func>
class local_future
{
    // Make sure the function type is callable
    typedef typename std::decay<Func>::type decay_func;
    static_assert(detail::is_callable<decay_func()>::value, "Invalid function type passed to local_spawn()");

    // Task result type
    typedef typename detail::remove_task<decltype(std::declval<decay_func>()())>::type result_type;
    typedef typename detail::void_to_fake_void<result_type>::type internal_result;

    // Task execution function type
    typedef detail::root_exec_func<internal_result,
                                   decay_func,
                                   detail::is_stage_future<decltype(std::declval<decay_func>()())>::value> exec_func;

    // Task object embedded directly. The ref-count is initialized to 1 so it
    // will never be freed using delete, only when the local_future is destroyed.
    std::shared_ptr<detail::task_func<exec_func, internal_result>> internal_task;

    // Friend access for local_spawn
    template<typename F>
    friend local_future<F> local_spawn(detail::scheduler &sched, F &&f);
    template<typename F>
    friend local_future<F> local_spawn(F &&f);

    // Constructor, used by local_spawn
    local_future(detail::scheduler &sched, Func &&f)
        : internal_task(std::make_shared<detail::task_func<exec_func, internal_result>>(std::forward<Func>(f)))
    {
        // Avoid an expensive ref-count modification since the task isn't shared yet
        detail::schedule_task(sched, detail::task_ptr(std::static_pointer_cast<detail::task_base>(internal_task)));
    }

public:
    // Non-movable and non-copyable
    local_future(const local_future &) = delete;
    local_future &operator=(const local_future &) = delete;

    // Wait for the task to complete when destroying
    ~local_future()
    {
        wait();

        // Now spin until the reference count drops to 1, since the scheduler
        // may still have a reference to the task.
        while (!internal_task.use_count() == 1) {
#if defined(__GLIBCXX__) && __GLIBCXX__ <= 20140612
            // Some versions of libstdc++ (4.7 and below) don't include a
            // definition of std::this_thread::yield().
            sched_yield();
#else
            std::this_thread::yield();
#endif
        }
    }

    // Query whether the task has finished executing
    bool ready() const
    {
        return internal_task->ready();
    }

    // Query whether the task has been canceled with an exception
    bool canceled() const
    {
        return internal_task->state.load(std::memory_order_acquire) == detail::task_state::canceled;
    }

    // Wait for the task to complete
    void wait()
    {
        internal_task->wait();
    }

    // Get the result of the task
    result_type get()
    {
        internal_task->wait_and_throw();
        return detail::fake_void_to_void(internal_task->get_result(stage_future<result_type>()));
    }

    // Get the exception associated with a canceled task
    std::exception_ptr get_exception() const
    {
        if (internal_task->wait() == detail::task_state::canceled)
            return internal_task.get_exception();
        else
            return std::exception_ptr();
    }
};

/////////////////////////////////////////////////////the global function////////////////////////////////////////////////////////////////
// supply_async a function asynchronously return not void value with the parameter sched
template<typename Func>
stage_future<typename detail::remove_task<typename std::result_of<typename std::decay<Func>::type()>::type>::type>
supply_async(detail::scheduler &sched, Func &&f);
// supply_async a function asynchronously return not void value with default sched
template<typename Func>
decltype(supply_async(::stagefuture::default_scheduler(), std::declval<Func>()))
supply_async(Func &&f);
// run_async a function asynchronously return void value with the parameter sched
template<typename Func>
stage_future<void> run_async(detail::scheduler &sched, Func &&f);
// run_async a function asynchronously return void value with default sched
template<typename Func>
stage_future<void> run_async(Func &&f);
// Create a completed task containing a value
template<typename T>
stage_future<typename std::decay<T>::type> make_future(T &&value);
template<typename T>
stage_future<T &> make_task(std::reference_wrapper<T> value);
inline stage_future<void> make_task();
// Create a canceled task containing an exception
template<typename T>
stage_future<T> make_exception_task(std::exception_ptr except);
/////////////////////////////////////////////////////the global function////////////////////////////////////////////////////////////////


template<typename Func>
stage_future<typename detail::remove_task<typename std::result_of<typename std::decay<Func>::type()>::type>::type>
shedule_task(detail::scheduler &sched, Func &&f)
{
    // Using result_of in the function return type to work around bugs in the Intel
    // C++ compiler.
    // Make sure the function type is callable
    typedef typename std::decay<Func>::type decay_func;
    static_assert(detail::is_callable<decay_func()>::value, "Invalid function type passed to supply_async()");

    // Create task
    typedef typename detail::void_to_fake_void<typename detail::remove_task<decltype(std::declval<decay_func>()())>::type>::type
        internal_result;
    typedef detail::root_exec_func<internal_result,
                                   decay_func,
                                   detail::is_stage_future<decltype(std::declval<decay_func>()())>::value> exec_func;
    stage_future<typename detail::remove_task<decltype(std::declval<decay_func>()())>::type> out;
    detail::task_ptr task_ptr(new detail::task_func<exec_func,
                                                    internal_result>(std::forward<Func>(f)));
    detail::set_internal_task(out, task_ptr);

    // Avoid an expensive ref-count modification since the task isn't shared yet
    detail::get_internal_task(out)->sched = std::addressof(sched);
    detail::schedule_task(sched, task_ptr);
    return out;
}

// supply_async a function asynchronously return not void value with the parameter sched
template<typename Func>
stage_future<typename detail::remove_task<typename std::result_of<typename std::decay<Func>::type()>::type>::type>
supply_async(detail::scheduler &sched, Func &&f)
{
    typedef
    typename detail::remove_task<typename std::result_of<typename std::decay<Func>::type()>::type>::type return_type;
    static_assert(!std::is_void<return_type>::value, "The type of the func's result is must not be void");
    return shedule_task(sched, std::forward<Func>(f));
}

template<typename Func>
decltype(supply_async(::stagefuture::default_scheduler(), std::declval<Func>()))
supply_async(Func &&f)
{
    //the type of the function's return value must not be void
    typedef
    typename detail::remove_task<typename std::result_of<typename std::decay<Func>::type()>::type>::type return_type;
    static_assert(!std::is_void<return_type>::value, "The type of the func's result is must not be void");
    return shedule_task(::stagefuture::default_scheduler(), std::forward<Func>(f));
}

template<typename Func>
stage_future<void> run_async(detail::scheduler &sched, Func &&f)
{
    typedef
    typename detail::remove_task<typename std::result_of<typename std::decay<Func>::type()>::type>::type return_type;
    //the type of the function's return value must be void
    static_assert(std::is_void<return_type>::value, "The type of the func's result is must be void");
    return shedule_task(sched, std::forward<Func>(f));
}

template<typename Func>
stage_future<void> run_async(Func &&f)
{
    //the type of the function's return value must be void
    typedef
    typename detail::remove_task<typename std::result_of<typename std::decay<Func>::type()>::type>::type return_type;
    static_assert(std::is_void<return_type>::value, "The type of the func's result is must be void");
    return shedule_task(::stagefuture::default_scheduler(), std::forward<Func>(f));
}

// Create a completed task containing a value
template<typename T>
stage_future<typename std::decay<T>::type> make_future(T &&value)
{
    stage_future<typename std::decay<T>::type> out;

    detail::set_internal_task(out, detail::task_ptr(new detail::task_result<typename std::decay<T>::type>));
    detail::get_internal_task(out)->set_result(std::forward<T>(value));
    detail::get_internal_task(out)->state.store(detail::task_state::completed, std::memory_order_relaxed);

    return out;
}
template<typename T>
stage_future<T &> make_task(std::reference_wrapper<T> value)
{
    stage_future<T &> out;

    detail::set_internal_task(out, detail::task_ptr(new detail::task_result<T &>));
    detail::get_internal_task(out)->set_result(value.get());
    detail::get_internal_task(out)->state.store(detail::task_state::completed, std::memory_order_relaxed);

    return out;
}
inline stage_future<void> make_task()
{
    stage_future<void> out;

    detail::set_internal_task(out, detail::task_ptr(new detail::task_result<detail::fake_void>));
    detail::get_internal_task(out)->state.store(detail::task_state::completed, std::memory_order_relaxed);

    return out;
}

// Create a canceled task containing an exception
template<typename T>
stage_future<T> make_exception_task(std::exception_ptr except)
{
    stage_future<T> out;

    detail::set_internal_task(out,
                              detail::task_ptr(new detail::task_result<typename detail::void_to_fake_void<T>::type>));
    detail::get_internal_task(out)->set_exception(std::move(except));
    detail::get_internal_task(out)->state.store(detail::task_state::canceled, std::memory_order_relaxed);

    return out;
}

// Spawn a very limited task which is restricted to the current function and
// joins on destruction. Because local_future is not movable, the result must
// be captured in a reference, like this:
// auto&& x = local_spawn(...);
template<typename Func>
local_future<Func> local_spawn(detail::scheduler &sched, Func &&f)
{
    // Since local_future is not movable, we construct it in-place and let the
    // caller extend the lifetime of the returned object using a reference.
    return {sched, std::forward<Func>(f)};
}
template<typename Func>
#ifdef __GNUC__
__attribute__((warn_unused_result))
#endif
local_future<Func> local_spawn(Func &&f)
{
    return {::stagefuture::default_scheduler(), std::forward<Func>(f)};
}

} // namespace stagefuture

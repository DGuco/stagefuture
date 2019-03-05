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

// Forward declarations
class task_run_handle;
class threadpool_scheduler;

// Scheduler interface:
// A scheduler is any type that implements this function:
// void schedule(stagefuture::task_run_handle t);
// This function should result in t.run() being called at some stage_future point.

namespace detail
{

class scheduler
{
public:
    virtual void schedule(task_run_handle t) = 0;
};

// Singleton scheduler classes
class thread_scheduler_impl
{
public:
    LIBASYNC_EXPORT static void schedule(task_run_handle t);
};

class inline_scheduler_impl: public detail::scheduler
{
public:
    void schedule(task_run_handle t);
};

// Reference counted pointer to task data
struct task_base;

typedef ref_count_ptr<task_base> task_ptr;

//
//// Helper function to schedule a task using a scheduler
void schedule_task(detail::scheduler &sched, task_ptr t);

// Wait for the given task to finish. This will call the wait handler currently
// active for this thread, which causes the thread to sleep by default.
LIBASYNC_EXPORT void wait_for_task(task_base *wait_task);

// Forward-declaration for data used by threadpool_scheduler
struct threadpool_data;

} // namespace detail

// Run a task in the current thread as soon as it is scheduled
inline detail::inline_scheduler_impl &inline_scheduler()
{
    static detail::inline_scheduler_impl instance;
    return instance;
}

// Run a task in a separate thread. Note that this scheduler does not wait for
// threads to finish at process exit. You must ensure that all threads finish
// before ending the process.
inline detail::thread_scheduler_impl &thread_scheduler()
{
    static detail::thread_scheduler_impl instance;
    return instance;
}

// Built-in thread pool scheduler with a size that is configurable from the
// LIBASYNC_NUM_THREADS environment variable. If that variable does not exist
// then the number of CPUs in the system is used instead.
LIBASYNC_EXPORT threadpool_scheduler &default_threadpool_scheduler();

// Default scheduler that is used when one isn't specified. This defaults to
// default_threadpool_scheduler(), but can be overriden by defining
// LIBASYNC_CUSTOM_DEFAULT_SCHEDULER before including async++.h. Keep in mind
// that in that case stagefuture::default_scheduler should be declared before
// including async++.h.
#ifndef LIBASYNC_CUSTOM_DEFAULT_SCHEDULER
inline threadpool_scheduler &default_scheduler()
{
    return default_threadpool_scheduler();
}
#endif

// Scheduler that holds a list of tasks which can then be explicitly executed
// by a thread. Both adding and running tasks are thread-safe operations.
class fifo_scheduler: public detail::scheduler
{
    struct internal_data;
    std::unique_ptr<internal_data> impl;

public:
    LIBASYNC_EXPORT fifo_scheduler();
    LIBASYNC_EXPORT ~fifo_scheduler();

    // Add a task to the queue
    LIBASYNC_EXPORT void schedule(task_run_handle t);

    // Try running one task from the queue. Returns false if the queue was empty.
    LIBASYNC_EXPORT bool try_run_one_task();

    // Run all tasks in the queue
    LIBASYNC_EXPORT void run_all_tasks();
};

// Scheduler that runs tasks in a work-stealing thread pool of the given size.
// Note that destroying the thread pool before all tasks have completed may
// result in some tasks not being executed.
class threadpool_scheduler: public detail::scheduler
{
    std::unique_ptr<detail::threadpool_data> impl;

public:
    //forbidden copy construct
    threadpool_scheduler(const threadpool_scheduler &other) = delete;
    //forbidden operator =
    threadpool_scheduler &operator=(const threadpool_scheduler &other) = delete;
    LIBASYNC_EXPORT threadpool_scheduler(threadpool_scheduler &&other);

    // Create a thread pool with the given number of threads
    LIBASYNC_EXPORT threadpool_scheduler(std::size_t num_threads);

    // Create a thread pool with the given number of threads. Call `prerun`
    // function before execution loop and `postrun` after.
    LIBASYNC_EXPORT threadpool_scheduler(std::size_t num_threads,
                                         std::function<void()> &&prerun_,
                                         std::function<void()> &&postrun_);

    // Destroy the thread pool, tasks that haven't been started are dropped
    LIBASYNC_EXPORT ~threadpool_scheduler();

    // Schedule a task to be run in the thread pool
    LIBASYNC_EXPORT void schedule(task_run_handle t);
};
} // namespace stagefuture

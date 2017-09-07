#include <stdarg.h>
#include <string.h>
#include <iostream>
#include "../inc/thread.h"

void* ThreadProc( void *pvArgs )
{
    if( !pvArgs )
    {
        return NULL;
    }
    
    CThread *pThread = (CThread *)pvArgs;
    
    if( pThread->PrepareToRun() )  // handle
    {
        return NULL;
    }
    
    pThread->Run();
    
    return NULL;
}

CThread::CThread()
{
    m_iRunStatus = RT_INIT;
}

CThread::~CThread()
{
}

int CThread::CreateThread()
{
    m_iRunStatus = RT_RUNNING;
    //创建线程
    mt = std::thread(ThreadProc, (void *)this );
    return 0;
}

int CThread::CondBlock()
{
    std::unique_lock<std::mutex> lk(m_condMut);
    // 线程被阻塞或者停止，这里的while等待主要防止多个线程等待时被意外唤醒，保证当条件满足时，只有一个线程在处理
    while( IsToBeBlocked() || m_iRunStatus == RT_STOPED )
    {
        if( m_iRunStatus == RT_STOPED )
        {
            //退出线程
            std::cout << "Thread exit,thread id " << std::this_thread::get_id() << std::endl;;
            pthread_exit(0);
        }
        m_iRunStatus = RT_BLOCKED;
        // 进入休眠状态,直到条件满足
        data_cond.wait(lk);
    }
    
    if( m_iRunStatus != RT_RUNNING )
    {
        std::cout << "Thread running,thread id " << std::this_thread::get_id() << std::endl;;
    }
    // 线程状态变为rt_running
    m_iRunStatus = RT_RUNNING;
    lk.unlock();
    return 0;
}

int CThread::WakeUp()
{
    // 该过程需要在线程锁内完成
    std::lock_guard<std::mutex> guard(m_condMut);
    
    if( !IsToBeBlocked() && m_iRunStatus == RT_BLOCKED )
    {
        // 向线程发出信号以唤醒
        data_cond.notify_one();
    }
    
    return 0;
}

int CThread::StopThread()
{
    std::lock_guard<std::mutex> guard(m_condMut);
    m_iRunStatus = RT_STOPED;
    data_cond.notify_one();
    // 等待该线程终止
    if (mt.joinable()) {
        mt.join();
    }
    return 0;
}



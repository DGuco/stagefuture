#ifndef _MY_THREAD_HPP_
#define _MY_THREAD_HPP_

#include <pthread.h>
#include <condition_variable>
#include <mutex>
#include <thread>

#define TRACE_DEBUG		ThreadLogDebug
#define TRACE_INFO			ThreadLogInfo
#define TRACE_NOTICE		ThreadLogNotice
#define TRACE_WARN		ThreadLogWarn
#define TRACE_ERROR		ThreadLogError
#define TRACE_FATAL		ThreadLogFatal


enum eRunStatus
{
    RT_INIT = 0,
    RT_BLOCKED = 1,
    RT_RUNNING = 2,
    RT_STOPED = 3
};

void* ThreadProc( void *pvArgs );

class CThread
{
public:
    CThread();
    virtual ~CThread();
    
    virtual int PrepareToRun() = 0;
    virtual int Run() = 0;
    virtual bool IsToBeBlocked() = 0;
    
    int CreateThread();
    int WakeUp();
    int StopThread();
    
protected:
    int CondBlock();
    
public:
    int m_iRunStatus;
    
private:
    std::mutex m_condMut;
    std::condition_variable data_cond;
    std::thread mt;
};


#endif

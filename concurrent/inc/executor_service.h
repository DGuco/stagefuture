//
//  executor_service.h
//  mythread
//
//  Created by 杜国超 on 17/9/7.
//
//
#ifndef EXECTOR_SERVICE_HPP
#define EXECTOR_SERVICE_HPP
#include <list>
#include "runnable.h"
#include "thread.h"

class CExecutorService : CExecutor
{
public:
    CExecutorService(int maxTask);
    ~CExecutorService();
private:
    std::list<CRunnable> m_lTaskList;
    int m_iMaxTask;
};

#endif /* EXECTOR_SERVICE_HPP */

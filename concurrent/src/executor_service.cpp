//
//  executor_service.h executor_service.cpp
//  mythread
//
//  Created by 杜国超 on 17/9/7.
//
//

#include <stdio.h>
#include "../inc/executor_service.h"

CExecutorService::CExecutorService(int maxTask) : m_iMaxTask(maxTask)
{
    m_lTaskList.clear();
}

CExecutorService::~CExecutorService()
{
    
}

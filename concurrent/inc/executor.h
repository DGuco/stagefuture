//
//  executor.h
//  mythread
//
//  Created by 杜国超 on 17/9/7.
//
//

#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "runnable.h"

class CExecutor
{
public:
    virtual void Execute(CRunnable command) = 0;
};

#endif /* EXECUTOR_H */

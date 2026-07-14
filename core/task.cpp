#include "task.h"
#include "thread_scheduler.h"

CTask::CTask(CSafePtr<CTaskScheduler> scheduler, std::string signature)
	:m_pScheduler(scheduler),
	m_TaskSignature(signature)
{
	SetState(enTaskState::eTaskInit);
	m_pArgsTypeList = NULL;
};

CTask::~CTask()
{
	try 
	{
		enTaskState bState = GetState();
		if(bState == enTaskState::eTaskDone || bState == enTaskState::eTaskFailed)
		{
			RunChildTask();
		}
		m_pArgsTypeList.Free();
	}
	catch(std::exception e)
	{
		if (m_pArgsTypeList != NULL)
		{
			m_pArgsTypeList.Free();
		}
	}
}

void CTask::AddChildTask(TaskPtr pTask)
{
	CSafeLock guard(m_childTaskLock);
	m_childTaskQueue.push(pTask);
}

void CTask::OnFinish()
{
	SetState(enTaskState::eTaskDone);
	RunChildTask();
}

void CTask::OnFailed() 
{
	SetState(enTaskState::eTaskFailed);
	RunChildTask();
	CACHE_LOG(THREAD_ERROR, "Task[{}] execute failed", m_TaskSignature);
}

void CTask::Run()
{
	try
	{
		SetState(enTaskState::eTaskDoing);
		SetStartTime(CTimeHelper::GetSingletonPtr()->GetMSTime());
		Execute();
		OnFinish();
	}
	catch (std::exception& e)
	{
		CACHE_LOG(THREAD_ERROR, "Task[{}] caught exception,exception msg:{}",m_TaskSignature,e.what());
		OnFailed();
	}
}

void CTask::RunChildTask()
{
	TaskPtr pTask;
	while (true)
	{
		{
			CSafeLock guard(m_childTaskLock);
			if (m_childTaskQueue.empty())
			{
				break;
			}
			pTask = m_childTaskQueue.front();
			m_childTaskQueue.pop();
		}
		if (pTask != NULL)
		{
			if(pTask->CombinedType() != enCombineType::eCombineNone)
			{
				pTask->CombineTaskDone(GetShared());
			}else
			{
				ExecuteChildTask(pTask);
			}
		}
	}
}

void CTask::SetAcceptCombineInfo(CSafePtr<IArgsTypeInfo> pArgs)
{
	if (m_pArgsTypeList != NULL)
	{
		ASSERT_EX(false, "This Task has combined once");
	}
	m_pArgsTypeList = pArgs;
}

void CTask::FillCombineTaskArgs(TaskPtr pChildTask)
{
	if (m_pArgsTypeList != NULL)
	{
		m_pArgsTypeList->FillWaitTaskParm(GetShared(),pChildTask);
	}
}

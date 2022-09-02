#include <framework/seadFramework.h>
#include <framework/seadMethodTreeMgr.h>
#include <framework/seadTaskBase.h>
#include <framework/seadTaskMgr.h>
#include <prim/seadSafeString.h>
#include <resource/seadResourceMgr.h>
#include <thread/seadDelegateThread.h>
#include "prim/seadScopedLock.h"

namespace sead
{
bool TaskMgr::changeTaskState_(TaskBase* task, TaskBase::State state)
{
    ScopedLock<CriticalSection> lock = makeScopedLock(mCriticalSection);

    TaskBase::State taskState = task->mState;
    if (taskState == state)
        return false;

    switch (state)
    {
    case TaskBase::cPrepare:
        if (taskState != TaskBase::cCreated)
            return false;

        task->mState = TaskBase::cPrepare;
        appendToList_(mPrepareList, task);

        if (mPrepareThread == nullptr)
            return false;

        return mPrepareThread->sendMessage(1, MessageQueue::BlockType::NonBlocking);

    case TaskBase::cPrepareDone:
        task->mState = TaskBase::cPrepareDone;
        task->mTaskListNode.erase();

        return true;

    case TaskBase::cRunning:
        task->mState = TaskBase::cRunning;
        task->mTaskListNode.erase();
        appendToList_(mActiveList, task);

        task->enterCommon();

        return true;

    case TaskBase::cDying:
        task->mState = TaskBase::cDying;

        return true;

    case TaskBase::cDestroyable:
        if (taskState != TaskBase::cRunning)
            return false;
            
        task->mState = TaskBase::cDestroyable;
        task->detachCalcImpl();
        task->detachDrawImpl();
        appendToList_(mDestroyableList, task);

        return true;

    case TaskBase::cDead:
        task->exit();
        task->mState = TaskBase::cDead;
        task->mTaskListNode.erase();

        return true;
    case TaskBase::cCreated:
    case TaskBase::cSleep:
        return false;
    }
}

void TaskMgr::destroyTaskSync(TaskBase* task)
{
    if (mParentFramework->mMethodTreeMgr->mCS.tryLock())
    {
        doDestroyTask_(task);
        mParentFramework->mMethodTreeMgr->mCS.unlock();
    }
}

// NON_MATCHING: mismatch in the loop of destroying heaps
void TaskMgr::doDestroyTask_(TaskBase* task)
{
    ScopedLock<CriticalSection> lock = makeScopedLock(mCriticalSection);

    TTreeNode<TaskBase*>* node = task->child();
    while (node != NULL)
    {
        doDestroyTask_(node->value());
        node = task->child();
    }

    if (changeTaskState_(task, TaskBase::cDead))
    {
        task->detachAll();

        HeapArray heapArray(task->mHeapArray);
        for (s32 i = 0; i < HeapMgr::getRootHeapNum(); i++)
        {
            Heap* heap = heapArray.getHeap(i);
            if (heap != nullptr)
                heap->destroy();
        }
    }
}

// NON_MATCHING: mismatch in the loop of destroying heaps
void TaskMgr::finalize()
{
    if (mPrepareThread != nullptr)
    {
        mPrepareThread->quitAndDestroySingleThread(false);
        delete mPrepareThread;
        mPrepareThread = nullptr;
    }

    if (mRootTask != nullptr)
    {
        destroyTaskSync(mRootTask);
        mRootTask = nullptr;
    }

    for (s32 i = 0; i < HeapMgr::getRootHeapNum(); i++)
    {
        Heap* heap = mHeapArray.mHeaps[i];
        if (heap)
        {
            heap->destroy();
            mHeapArray.mHeaps[i] = nullptr;
        }
    }
}

}  // namespace sead

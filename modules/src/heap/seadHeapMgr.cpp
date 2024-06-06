#include <heap/seadExpHeap.h>
#include <heap/seadHeap.h>
#include <heap/seadHeapMgr.h>
#include <prim/seadScopedLock.h>
#include <thread/seadThread.h>
#include <time/seadTickSpan.h>
#include <utility>

namespace sead
{
Arena* HeapMgr::sArena = nullptr;
HeapMgr* HeapMgr::sInstancePtr = nullptr;

HeapMgr HeapMgr::sInstance;
Arena HeapMgr::sDefaultArena;
HeapMgr::RootHeaps HeapMgr::sRootHeaps;
HeapMgr::IndependentHeaps HeapMgr::sIndependentHeaps;
CriticalSection HeapMgr::sHeapTreeLockCS;
TickSpan HeapMgr::sSleepSpanAtRemoveCacheFailure;

HeapMgr::HeapMgr() = default;
HeapMgr::~HeapMgr() = default;

void HeapMgr::initialize(size_t size)
{
    sHeapTreeLockCS.lock();
    sArena = &sDefaultArena;
    sDefaultArena.initialize(size);
    initializeImpl_();
    sHeapTreeLockCS.unlock();
}

void HeapMgr::initializeImpl_()
{
    sInstance.mAllocFailedCallback = nullptr;
    sSleepSpanAtRemoveCacheFailure = TickSpan::makeFromMicroSeconds(10);
    createRootHeap_();
    sInstancePtr = &sInstance;
}

void HeapMgr::initialize(Arena* arena)
{
    sArena = arena;
    initializeImpl_();
}

void HeapMgr::createRootHeap_()
{
    auto* expHeap = ExpHeap::tryCreate(sArena->mStart, sArena->mSize, "RootHeap", false);
    sRootHeaps.pushBack(expHeap);
}

Heap* HeapMgr::findContainHeap(const void* memBlock) const
{
    FindContainHeapCache* heapCache = nullptr;

    ThreadMgr* threadMgr = ThreadMgr::instance();
    if (threadMgr)
    {
        Thread* currentThread = threadMgr->getCurrentThread();
        if (currentThread)
            heapCache = currentThread->getFindContainHeapCache();
    }

    if (heapCache)
    {
        FindContainHeapCacheAccessor heapAccessor(&heapCache->mHeap);

        Heap* heap = heapAccessor.getHeap();
        if (heap && heap->hasNoChild_() && heap->isInclude(memBlock))
        {
#ifdef SEAD_DEBUG
            heapCache->nolockhit++;
#endif // SEAD_DEBUG
            return heap;
        }
    }

    ScopedLock<CriticalSection> lock(&sHeapTreeLockCS);

    if (heapCache)
    {
#ifdef SEAD_DEBUG
        heapCache->call++;
#endif // SEAD_DEBUG

        Heap* heap = heapCache->getHeap();
        if (heap)
        {
            Heap* containHeap = heap->findContainHeap_(memBlock);
            if (containHeap)
            {
                if (containHeap == heap)
                {
#ifdef SEAD_DEBUG
                    heapCache->hit++;
#endif // SEAD_DEBUG
                }
                else
                {
#ifdef SEAD_DEBUG
                    heapCache->miss++;
#endif // SEAD_DEBUG
                    heapCache->setHeap(containHeap);
                }

                return containHeap;
            }
        }
    }

    for (Heap& heap : sRootHeaps)
    {
        Heap* containHeap = heap.findContainHeap_(memBlock);
        if (containHeap)
        {
            if (heapCache)
            {
#ifdef SEAD_DEBUG
                heapCache->miss++;
#endif // SEAD_DEBUG
                heapCache->setHeap(containHeap);
            }

            return containHeap;
        }
    }

    for (Heap& heap : sIndependentHeaps)
    {
        Heap* containHeap = heap.findContainHeap_(memBlock);
        if (containHeap)
        {
            if (heapCache)
            {
#ifdef SEAD_DEBUG
                heapCache->miss++;
#endif // SEAD_DEBUG
                heapCache->setHeap(containHeap);
            }

            return containHeap;
        }
    }

#ifdef SEAD_DEBUG
    if (heapCache)
        heapCache->notfound++;
#endif // SEAD_DEBUG

    return nullptr;
}

void HeapMgr::destroy()
{
    sHeapTreeLockCS.lock();
    sInstance.mAllocFailedCallback = nullptr;

    while (!sIndependentHeaps.isEmpty())
    {
        sIndependentHeaps.back()->destroy();
        sIndependentHeaps.popBack();
    }

    while (!sRootHeaps.isEmpty())
    {
        sRootHeaps.back()->destroy();
        sRootHeaps.popBack();
    }

    sInstancePtr = nullptr;
    sArena->destroy();
    sArena = nullptr;
    sHeapTreeLockCS.unlock();
}

void HeapMgr::initHostIO() {}

bool HeapMgr::isContainedInAnyHeap(const void* ptr)
{
    for (auto& heap : sRootHeaps)
    {
        if (heap.isInclude(ptr))
            return true;
    }
    for (auto& heap : sIndependentHeaps)
    {
        if (heap.isInclude(ptr))
            return true;
    }
    return false;
}
void HeapMgr::dumpTreeYAML(WriteStream& stream)
{
    sHeapTreeLockCS.lock();
    for (auto& heap : sRootHeaps)
    {
        heap.dumpTreeYAML(stream, 0);
    }
    for (auto& heap : sIndependentHeaps)
    {
        heap.dumpTreeYAML(stream, 0);
    }
    sHeapTreeLockCS.unlock();
}

void HeapMgr::setAllocFromNotSeadThreadHeap(Heap* heap)
{
    mAllocFromNotSeadThreadHeap = heap;
}

void HeapMgr::removeFromFindContainHeapCache_(Heap* heap)
{
    auto* threadMgr = ThreadMgr::instance();
    if (!threadMgr)
        return;

    Thread* mainThread = threadMgr->getMainThread();
    if (mainThread)
    {
        while (!mainThread->getFindContainHeapCache()->tryRemoveHeap(heap))
            Thread::sleep(sSleepSpanAtRemoveCacheFailure);
    }

    while (threadMgr->tryRemoveFromFindContainHeapCache(heap))
        Thread::sleep(sSleepSpanAtRemoveCacheFailure);
}

Heap* HeapMgr::findHeapByName(const sead::SafeString& name, int index) const
{
    auto lock = makeScopedLock(sHeapTreeLockCS);
    for (auto& heap : sRootHeaps)
    {
        Heap* found = findHeapByName_(&heap, name, &index);
        if (found)
            return found;
    }
    for (auto& heap : sIndependentHeaps)
    {
        Heap* found = findHeapByName_(&heap, name, &index);
        if (found)
            return found;
    }
    return nullptr;
}

Heap* HeapMgr::findHeapByName_(Heap* heap, const SafeString& name, int* index)
{
    if (heap->getName() == name)
    {
        if (*index == 0)
            return heap;
        --*index;
    }
    for (auto& child : heap->mChildren)
    {
        Heap* found = findHeapByName_(&child, name, index);
        if (found)
            return found;
    }
    return nullptr;
}

static Heap* currentHeap = nullptr;

Heap* HeapMgr::getCurrentHeap() const
{
    return currentHeap;
    /*Thread* currentThread = ThreadMgr::instance()->getCurrentThread();
    printf("CurrentThread: %p, currentHeap: %p\n", currentThread, currentThread ? currentThread->getCurrentHeap() : nullptr);
    if (currentThread)
        return currentThread->getCurrentHeap();
    return mAllocFromNotSeadThreadHeap;*/
}

Heap* HeapMgr::setCurrentHeap_(Heap* heap)
{
    Heap* prevHeap = currentHeap;
    currentHeap = heap;
    return prevHeap;
    //return ThreadMgr::instance()->getCurrentThread()->setCurrentHeap(heap);
}

void HeapMgr::removeRootHeap(Heap* heap)
{
    if (sRootHeaps.size() < 1)
        return;
    s32 index = sRootHeaps.indexOf(heap);
    if (index != -1)
        sRootHeaps.erase(index);
}

HeapMgr::IAllocFailedCallback*
HeapMgr::setAllocFailedCallback(HeapMgr::IAllocFailedCallback* callback)
{
    return std::exchange(mAllocFailedCallback, callback);
}

FindContainHeapCache::FindContainHeapCache() = default;

bool FindContainHeapCache::tryRemoveHeap(Heap* heap)
{
    uintptr_t original;
    if (mHeap.compareExchange(uintptr_t(heap), 0, &original))
        return true;
    return (original & ~1u) != uintptr_t(heap);
}
}  // namespace sead

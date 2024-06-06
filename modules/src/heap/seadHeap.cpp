#include <heap/seadHeap.h>
#include <heap/seadHeapMgr.h>
#include <prim/seadScopedLock.h>

namespace sead
{
Heap::Heap(const SafeString& name, Heap* parent, void* start, size_t size, HeapDirection direction, bool enableLock)
    : IDisposer(parent, HeapNullOption::UseSpecifiedOrContainHeap)
    , INamable(name)
    , mStart(start)
    , mSize(size)
    , mParent(parent)
    , mChildren()
    , mListNode()
    , mDisposerList()
    , mDirection(direction)
    , mCS(parent)
#ifdef SEAD_DEBUG
    , mFlag((1 << Flag::eEnableWarning) | (1 << Flag::eEnableDebugFillSystem) | (1 << Flag::eEnableDebugFillUser))
#else
    , mFlag(1 << Flag::cEnableWarning)
#endif // SEAD_DEBUG
    //, mHeapCheckTag(static_cast<u16>(HeapMgr::getHeapCheckTag()))
#ifdef SEAD_DEBUG
    , mAccessThread(nullptr)
#endif // SEAD_DEBUG
{
    mFlag.changeBit(Flag::cEnableLock, enableLock);

    ConditionalScopedLock<CriticalSection> lock(&mCS, isEnableLock());

    mChildren.initOffset(offsetof(Heap, mListNode));
    mDisposerList.initOffset(IDisposer::getListNodeOffset());
}

void Heap::pushBackChild_(Heap* child)
{
    ScopedLock<CriticalSection> treeLock(HeapMgr::getHeapTreeLockCS_());
    ConditionalScopedLock<CriticalSection> heapLock(&mCS, isEnableLock());

    mChildren.pushBack(child);
}

Heap::~Heap() = default;

void Heap::appendDisposer_(IDisposer* disposer)
{
    ConditionalScopedLock<CriticalSection> lock(&mCS, isLockEnabled());
    mDisposerList.pushBack(disposer);
}

void Heap::removeDisposer_(IDisposer* disposer)
{
    ConditionalScopedLock<CriticalSection> lock(&mCS, isLockEnabled());
    mDisposerList.erase(disposer);
}

Heap* Heap::findContainHeap_(const void* ptr)
{
    if (!isInclude(ptr))
        return nullptr;

    for (auto it = mChildren.begin(); it != mChildren.end(); ++it)
    {
        if (it->isInclude(ptr))
            return it->findContainHeap_(ptr);
    }

    return this;
}

}  // namespace sead

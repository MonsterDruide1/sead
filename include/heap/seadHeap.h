#ifndef SEAD_HEAP_H_
#define SEAD_HEAP_H_

#include <stddef.h>

#include <basis/seadRawPrint.h>
#include <basis/seadTypes.h>
#include <container/seadOffsetList.h>
#include <heap/seadDisposer.h>
#include <hostio/seadHostIOReflexible.h>
#include <prim/seadBitFlag.h>
#include <prim/seadEnum.h>
#include <prim/seadNamable.h>
#include <prim/seadRuntimeTypeInfo.h>
#include <prim/seadSafeString.h>
#include <thread/seadCriticalSection.h>

namespace sead
{
class Thread;
class WriteStream;

namespace hostio
{
class Context;
class PropertyEvent;
}  // namespace hostio

class Heap : public IDisposer, public INamable, public hostio::Reflexible
{
public:
    SEAD_ENUM(Flag, cEnableLock, cDisposing, cEnableWarning, cEnableDebugFillSystem,
              cEnableDebugFillUser)

    enum HeapDirection
    {
        cHeapDirection_Forward = 1,
        cHeapDirection_Reverse = -1
    };

    Heap(const SafeString& name, Heap* parent, void* address, size_t size, HeapDirection direction,
         bool);
    ~Heap() override;

    SEAD_RTTI_BASE(Heap)

    virtual void destroy() = 0;
    virtual size_t adjust() = 0;
    virtual void* tryAlloc(size_t size, s32 alignment) = 0;
    virtual void free(void* ptr) = 0;
#if SEAD_HEAP_FREEANDGETALLOCATABLESIZE_VIRTUAL
    virtual size_t freeAndGetAllocatableSize(void* ptr, s32) = 0;
#endif
    virtual void* resizeFront(void*, size_t) = 0;
    virtual void* resizeBack(void*, size_t) = 0;
    virtual void* tryRealloc(void* ptr, size_t size, s32 alignment);
    virtual void freeAll() = 0;
    virtual const void* getStartAddress() const = 0;
    virtual const void* getEndAddress() const = 0;
    virtual size_t getSize() const = 0;
    virtual size_t getFreeSize() const = 0;
    virtual size_t getMaxAllocatableSize(int alignment) const = 0;
    virtual bool isInclude(const void*) const = 0;
    virtual bool isEmpty() const = 0;
    virtual bool isFreeable() const = 0;
    virtual bool isResizable() const = 0;
    virtual bool isAdjustable() const = 0;

    virtual void dump() const {}
    virtual void dumpYAML(WriteStream& stream, int) const;
    void dumpTreeYAML(WriteStream& stream, int) const;

    bool lock()
    {
        if (!isEnableLock())
            return false;

        mCS.lock();
        return true;
    }
    bool unlock()
    {
        if (!isEnableLock())
            return false;

        mCS.unlock();
        return true;
    }

#ifdef SEAD_DEBUG
    void listenPropertyEvent(const hostio::PropertyEvent* event) override;
    void genMessage(hostio::Context*) override;
#endif
    virtual void genInformation_(hostio::Context*);
    virtual void makeMetaString_(BufferedSafeString*);

    virtual void pushBackChild_(Heap* child);

    void appendDisposer_(IDisposer* disposer);
    void removeDisposer_(IDisposer* disposer);
    Heap* findContainHeap_(const void* ptr);
    void destruct_();
    void dispose_(const void* begin, const void* end);
    void eraseChild_(Heap* child);
    bool hasNoChild_() const { return mChildren.size() == 0; }

    void* alloc(size_t size, s32 alignment = sizeof(void*))
    {
        void* ptr = tryAlloc(size, alignment);
        SEAD_ASSERT_MSG(ptr,
                        "alloc failed. size: %zu, allocatable size: %zu, alignment: %d, heap: %s",
                        size, getMaxAllocatableSize(alignment), alignment, getName().cstr());
        return ptr;
    }

    void enableLock(bool on) { mFlag.changeBit(Flag::cEnableLock, on); }
    void enableWarning(bool on) { mFlag.changeBit(Flag::cEnableWarning, on); }
    void enableDebugFillSystem(bool on) { mFlag.changeBit(Flag::cEnableDebugFillSystem, on); }
    void enableDebugFillUser(bool on) { mFlag.changeBit(Flag::cEnableDebugFillUser, on); }

    bool isLockEnabled() const { return mFlag.isOnBit(Flag::cEnableLock); }
    bool isWarningEnabled() const { return mFlag.isOnBit(Flag::cEnableWarning); }
    bool isDebugFillSystemEnabled() const { return mFlag.isOnBit(Flag::cEnableDebugFillSystem); }
    bool isDebugFillUserEnabled() const { return mFlag.isOnBit(Flag::cEnableDebugFillUser); }

    sead::CriticalSection& getCriticalSection() { return mCS; }
    HeapDirection getDirection() const { return mDirection; }
    bool isEnableLock() const { return mFlag.isOnBit(Flag::cEnableLock); }

    using HeapList = OffsetList<Heap>;
    using DisposerList = OffsetList<IDisposer>;

    void* mStart;
    size_t mSize;
    Heap* mParent;
    HeapList mChildren;
    ListNode mListNode;
    DisposerList mDisposerList;
    HeapDirection mDirection;
    CriticalSection mCS;
    BitFlag16 mFlag;
    u16 mHeapCheckTag;
#ifdef SEAD_DEBUG
    sead::Thread* mAccessThread;
#endif
};

inline void* Heap::tryRealloc(void*, size_t, s32)
{
    SEAD_ASSERT_MSG(false, "tryRealloc is not implement.");
    return nullptr;
}

}  // namespace sead

#endif  // SEAD_HEAP_H_

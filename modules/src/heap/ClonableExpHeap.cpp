#include "heap/ClonableExpHeap.h"
#include <cstdlib>
#include <utility>
#include "basis/seadRawPrint.h"
#include "basis/seadTypes.h"
#include "container/seadBuffer.h"
#include "heap/seadHeapMgr.h"
#include "heap/seadMemBlock.h"
#include "math/seadMathCalcCommon.h"

namespace sead {

ClonableExpHeap* ClonableExpHeap::create(size_t size, const SafeString& name, Heap* parent, s32 alignment, HeapDirection direction, bool enableLock)
{
    ClonableExpHeap* heap = ClonableExpHeap::tryCreate(size, name, parent, alignment, direction, enableLock);

#ifdef SEAD_DEBUG
    if (!heap)
    {
        if (!parent)
            parent = HeapMgr::instance()->getCurrentHeap();

        if (!parent)
        {
            SEAD_ASSERT_MSG(false, "heap create failed. [%s] size: %zu, parent: --(0x0)", name.cstr(), size);
        }
        else
        {
            SEAD_ASSERT_MSG(false, "heap create failed. [%s] size: %zu, parent: %s(0x%p), parent allocatable size: %zu",
                            name.cstr(), size, parent->getName().cstr(), parent, parent->getMaxAllocatableSize());
        }
    }
#endif // SEAD_DEBUG

    return heap;
}

ClonableExpHeap* ClonableExpHeap::tryCreate(size_t size_, const SafeString& name, Heap* parent, s32 alignment, HeapDirection direction, bool enableLock)
{
    if (!parent)
    {
        parent = HeapMgr::instance()->getCurrentHeap();
        if (!parent)
        {
            SEAD_ASSERT_MSG(false, "current heap is null");
            return nullptr;
        }
    }

    size_t size;
    if (size_ == 0)
    {
        size = parent->getMaxAllocatableSize(alignof(void*));
        SEAD_ASSERT_MSG(MathSizeT::isMultiplePow2(size, alignment),
                        "[bug] getMaxAllocatableSize must return multiple of cMinAlignment");
        size = MathSizeT::roundDownPow2(size, alignment);
    }
    else
    {
        size = MathSizeT::roundUpPow2(size_, alignment);
    }

    if (size < sizeof(ExpHeap) + sizeof(MemBlock) + 0x1)
    {
        SEAD_ASSERT_MSG(size_ == 0, "size must be able to include manage area: size=%zu", size);
        return nullptr;
    }

    void* start;

    {
#ifdef SEAD_DEBUG
        ScopedDebugFillSystemDisabler disabler(parent);
#endif // SEAD_DEBUG
        start = parent->tryAlloc(size, static_cast<s32>(direction) * alignment);
    }

    if (!start) {
        SEAD_ASSERT_MSG(false, "failed to allocate memory: size=%zu", size);
        return nullptr;
    }

    if (parent->getDirection() == HeapDirection::cHeapDirection_Reverse)
        direction = static_cast<HeapDirection>(-static_cast<s32>(direction));

    ClonableExpHeap* heap;
    if (direction == HeapDirection::cHeapDirection_Forward)
    {
        heap = new(start) ClonableExpHeap(name, parent, start, size, direction, enableLock);
    }
    else
    {
        void* ptr = PtrUtil::addOffset(start, size - sizeof(ExpHeap));
        heap = new(ptr) ClonableExpHeap(name, parent, start, size, direction, enableLock);
    }

    ClonableExpHeap::doCreate(heap, parent);

    return heap;
}

ClonableExpHeap* ClonableExpHeap::tryCreate(void* start, size_t size, const SafeString& name, bool enableLock)
{
    size = MathSizeT::roundDownPow2(size, cMinAlignment);
    if (size < sizeof(ClonableExpHeap) + sizeof(MemBlock) + 0x1)
    {
        SEAD_ASSERT_MSG(false, "size must be able to include manage area: size=%zu", size);
        return nullptr;
    }

    ClonableExpHeap* heap = new(start) ClonableExpHeap(name, nullptr, start, size, HeapDirection::cHeapDirection_Forward, enableLock);
    ClonableExpHeap::doCreate(heap, nullptr);

    return heap;
}

struct MemoryArea {
    u64 start;
    u64 end;
};

int comparePairSort(const std::pair<MemoryArea, u64>* a, const std::pair<MemoryArea, u64>* b) {
    return a->first.start - b->first.start;
}

int comparePairFind(const std::pair<MemoryArea, u64>* checkArea, const std::pair<MemoryArea, u64>* comparisonPointer) {
    if(checkArea->first.start > comparisonPointer->first.start)
        return 1;
    if(checkArea->first.end < comparisonPointer->first.start)
        return -1;
    return 0;  // fulfills start <= pointer < end
}

ClonableExpHeap* ClonableExpHeap::clone() const {
    u64 origRangeStart = (u64)mStart;
    u64 origRangeEnd = (u64)PtrUtil::addOffset(mStart, mSize);

    ClonableExpHeap* copy = create(mSize, getName(), mParent, 16, mDirection, isEnableLock());
    u64 copyRangeStart = (u64)copy->mStart;

    // use malloc to avoid placing on heap itself
    s32 numAllocs = mUseList.size();
    std::pair<MemoryArea, u64>* allocMappingsBuffer = (std::pair<MemoryArea, u64>*)malloc(numAllocs * sizeof(std::pair<MemoryArea, u64>));
    sead::Buffer<std::pair<MemoryArea, u64>> allocationMappings{numAllocs, allocMappingsBuffer};

    MemBlock* current = mUseList.front();
    int i=0;
    do {
        u64 startOfCopyBlock = (u64)copy->tryAlloc(current->getSize(), 8);

        allocationMappings[i++] = std::make_pair(MemoryArea{(u64)current->memory(), (u64)current->memory()+current->getSize()}, startOfCopyBlock);
    } while((current = mUseList.next(current)) != nullptr);

    allocationMappings.heapSort(0, numAllocs-1, &comparePairSort);

    for(int i=0; i<allocationMappings.size(); i++) {
        std::pair<MemoryArea, u64> pair = allocationMappings[i];
        MemoryArea& block = pair.first;
        u64 startOfCopyBlock = pair.second;
        u64 startOfOrigBlock = block.start;

        for(u64 i=0; i+7<block.end-block.start; i+=8) {
            u64 value = *((u64*)(startOfOrigBlock+i));
            if(value >= origRangeStart && value < origRangeEnd) {
                MemoryArea searchBlock = {value, (u64)-1};
                s32 mappingIndex = allocationMappings.binarySearch(std::make_pair(searchBlock, -1), &comparePairFind);
                if(mappingIndex == -1)
                    SEAD_ASSERT_MSG(false, "could not find mapping for %p, required at %p (base=%p)\n", (void*)value, (void*)(startOfOrigBlock+i), (void*)startOfOrigBlock);
                u64 offset = value - allocationMappings[mappingIndex].first.start;
                value = allocMappingsBuffer[mappingIndex].second + offset;
            }
            *((u64*)(startOfCopyBlock+i)) = value;
        }
        // copy remaining bytes
        for(u64 i=(block.end-block.start)/8*8; i<block.end-block.start; i++) {
            *((u8*)(startOfCopyBlock+i)) = *((u8*)(startOfOrigBlock+i));
        }
    }

    ::free(allocMappingsBuffer);
    return copy;
}

}

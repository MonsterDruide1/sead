#pragma once

#include "controller/seadControlDevice.h"
#include "controller/seadController.h"
#include "heap/seadHeap.h"
#include "thread/seadCriticalSection.h"
#include "thread/seadThread.h"

namespace nn::hid {
    struct VibrationDeviceHandle;
    struct VibrationValue;
    enum NpadJoyHoldType {};
}

namespace sead
{
// stubbed and only added methods for seadControllerMgr
class NinJoyNpadDevice : public ControlDevice
{
public:
    class VibrationThread : public Thread {
    public:
        VibrationThread(Heap* heap);
        ~VibrationThread() override;

        void requestVibration(const nn::hid::VibrationDeviceHandle& deviceHandle, const nn::hid::VibrationValue& value);

    protected:
        void calc_(MessageQueue::Element msg) override;

    private:
        void* gaps[43];
        CriticalSection mCS;
    };


    NinJoyNpadDevice(ControllerMgr* mgr, Heap* heap);

    void calc() override;

private:
    s32 mNpadIdUpdateNum;
    nn::hid::NpadJoyHoldType mNpadJoyHoldType;
    void* filler[4208];
    VibrationThread mVibrationThread;
};

}  // namespace sead

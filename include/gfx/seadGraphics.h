#pragma once

namespace sead::Graphics
{
enum DevicePosture : unsigned int
{
    cDevicePosture_Same = 0,
    cDevicePosture_RotateRight = 1,
    cDevicePosture_RotateLeft = 2,
    cDevicePosture_RotateHalfAround = 3,
    cDevicePosture_FlipX = 4,
    cDevicePosture_FlipY = 5,
    cDevicePosture_FlipXY = 3,
    cDevicePosture_Invalid = 4,
};

extern DevicePosture sDevicePosture;

}  // namespace sead::Graphics

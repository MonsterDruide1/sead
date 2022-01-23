#include "gfx/seadViewport.h"

#include <cmath>
#include "gfx/seadFrameBuffer.h"
#include "gfx/seadGraphics.h"
#include "math/seadBoundBox.h"
#include "math/seadVector.h"

namespace sead
{

Viewport::Viewport() : mDevicePosture(Graphics::sDevicePosture) {}
Viewport::Viewport(float x0, float y0, float x1, float y1)
    : BoundBox2f(x0, y0, x0 + x1, y0 + y1), mDevicePosture(Graphics::sDevicePosture)
{
}
Viewport::Viewport(const sead::BoundBox2f& bound_box)
    : BoundBox2f(bound_box), mDevicePosture(Graphics::sDevicePosture)
{
}
Viewport::Viewport(const LogicalFrameBuffer& frame_buffer)
    : mDevicePosture(Graphics::sDevicePosture)
{
    setByFrameBuffer(
        frame_buffer);  // mismatch: Probably due to bad comparing functions in this function
}
void Viewport::setByFrameBuffer(const LogicalFrameBuffer& frame_buffer)
{
    // requires this exact setup of min/max calls and argument order for matching.
    switch (mDevicePosture)
    {
    case Graphics::cDevicePosture_Same:
    case Graphics::cDevicePosture_FlipY:
    case Graphics::cDevicePosture_FlipX:
    case Graphics::cDevicePosture_RotateHalfAround:
        set({std::min(frame_buffer.getVirtualSize().x, 0.0F),
             std::min(frame_buffer.getVirtualSize().y, 0.0F)},
            {fmaxf(frame_buffer.getVirtualSize().x, 0.0F),
             fmaxf(frame_buffer.getVirtualSize().y, 0.0F)});
        break;
    case Graphics::cDevicePosture_RotateRight:
    case Graphics::cDevicePosture_RotateLeft:
        set({std::min(frame_buffer.getVirtualSize().y, 0.0F),
             std::min(frame_buffer.getVirtualSize().x, 0.0F)},
            {fmaxf(frame_buffer.getVirtualSize().y, 0.0F),
             fmaxf(frame_buffer.getVirtualSize().x, 0.0F)});
        break;
    default:
        break;
    }
}

}  // namespace sead

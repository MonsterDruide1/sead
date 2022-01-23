#pragma once

#include "gfx/seadProjection.h"
#include "math/seadBoundBox.h"
#include "math/seadVector.h"

namespace sead
{
class LogicalFrameBuffer;
class DrawContext;
class Camera;

class Viewport : public sead::BoundBox2f
{
public:
    Viewport();
    Viewport(float, float, float, float);
    Viewport(const sead::BoundBox2f&);
    Viewport(const sead::LogicalFrameBuffer&);
    virtual ~Viewport();

    void setByFrameBuffer(const sead::LogicalFrameBuffer&);
    void apply(sead::DrawContext*, const sead::LogicalFrameBuffer&); //missing `sead::DrawContext` and pfnc_functions
    void getOnFrameBufferPos(sead::Vector2f*, const sead::LogicalFrameBuffer&) const;
    void getOnFrameBufferSize(sead::Vector2f*, const sead::LogicalFrameBuffer&) const;
    void applyViewport(sead::DrawContext*, const sead::LogicalFrameBuffer&); //missing `sead::DrawContext` and pfnc_functions
    void applyScissor(sead::DrawContext*, const sead::LogicalFrameBuffer&); //missing `sead::DrawContext` and pfnc_functions
    void project(sead::Vector2f*, const sead::Vector3f&);
    void project(sead::Vector2f*, const sead::Vector2f&);
    void unproject(sead::Vector3f*, const sead::Vector2f&, const sead::Projection&,
                   const sead::Camera&);
    // void unprojectRay(sead::Ray<sead::Vector3f>*, const sead::Vector2f&, const sead::Projection&,
    // const sead::Camera&);

private:
    Graphics::DevicePosture mDevicePosture;
};

}  // namespace sead

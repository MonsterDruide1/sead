#include "gfx/seadCamera.h"
#include "basis/seadRawPrint.h"

namespace sead
{
Camera::~Camera() = default;

LookAtCamera::~LookAtCamera() = default;

LookAtCamera::LookAtCamera(const Vector3f& pos, const Vector3f& at, const Vector3f& up)
    : mPos(pos), mAt(at), mUp(up)
{
    SEAD_ASSERT(mPos != mAt);
    mUp.normalize();
}

OrthoCamera::~OrthoCamera() = default;

DirectCamera::~DirectCamera() = default;

void LookAtCamera::doUpdateMatrix(Matrix34f* mtx) const
{
    if (mPos == mAt)
        return;

    sead::Vector3f diff = mPos - mAt;
    diff.normalize();

    sead::Vector3f diff_up;
    diff_up.setCross(mUp, diff);
    diff_up.normalize();

    sead::Vector3f diff_diffUp;
    diff_diffUp.setCross(diff, diff_up);
    mtx->m[0][0] = diff_up.x;
    mtx->m[0][1] = diff_up.y;
    mtx->m[0][2] = diff_up.z;

    mtx->m[1][0] = diff_diffUp.x;
    mtx->m[1][1] = diff_diffUp.y;
    mtx->m[1][2] = diff_diffUp.z;

    mtx->m[2][0] = diff.x;
    mtx->m[2][1] = diff.y;
    mtx->m[2][2] = diff.z;

    mtx->m[0][3] = -diff_up.dot(mPos);
    mtx->m[1][3] = -diff_diffUp.dot(mPos);
    mtx->m[2][3] = -diff.dot(mPos);
}
}  // namespace sead

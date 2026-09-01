#include "fx_classes.hpp"

/* Source: CoDUOMP.exe 0x004a24b0..0x004a24c4.
 * Name: same-module Mac symbol COrientedParticle::Cull. */
qboolean COrientedParticle::Cull()
{
    return SFxHelper_CullSphere(renderEntity.origin, renderEntity.radius);
}

/* Source: CoDUOMP.exe 0x004a2680..0x004a269f.
 * Name: same-module Mac symbol CLine::Cull. */
qboolean CLine::Cull()
{
    return SFxHelper_CullCylinder(renderEntity.origin, renderEntity.oldorigin,
                                  renderEntity.radius, renderEntity.radius);
}

/* Source: CoDUOMP.exe 0x004a2840..0x004a2842.
 * Name: same-module Mac symbol CElectricity::Cull. */
qboolean CElectricity::Cull()
{
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004a2ab0..0x004a2acf.
 * Name: same-module Mac symbol CTail::Cull. */
qboolean CTail::Cull()
{
    return SFxHelper_CullCylinder(renderEntity.origin, renderEntity.oldorigin,
                                  renderEntity.radius, renderEntity.radius);
}

/* Source: CoDUOMP.exe 0x004a2f50..0x004a2f73.
 * Name: same-module Mac symbol CCylinder::Cull. */
qboolean CCylinder::Cull()
{
    return SFxHelper_CullCylinder(renderEntity.origin, renderEntity.oldorigin,
                                  renderEntity.radius2, renderEntity.radius);
}

/* Source: CoDUOMP.exe 0x004a3180..0x004a3182.
 * Name: same-module Mac symbol CEmitter::Cull. */
qboolean CEmitter::Cull()
{
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004a3d40..0x004a3d54.
 * Name: same-module Mac symbol CLight::Cull. */
qboolean CLight::Cull()
{
    return SFxHelper_CullSphere(renderEntity.origin, renderEntity.radius);
}

/* Source: CoDUOMP.exe 0x004a4190..0x004a4192.
 * Class binding: Windows vtable 0x005a2be8. */
qboolean CQuad::Cull()
{
    return qfalse;
}

/* Source: CoDUOMP.exe 0x004a46c0..0x004a46d1. The decal is bounded around the
 * CEffect origin rather than its renderer entity origin. */
qboolean CDecal::Cull()
{
    return SFxHelper_CullSphere(origin, renderEntity.radius);
}

/* Source: CoDUOMP.exe 0x004aee90..0x004aee92.
 * Name and class binding: Windows vtable 0x005a2bcc and same-module Mac symbol
 * CFlash::Cull. */
qboolean CFlash::Cull()
{
    return qfalse;
}

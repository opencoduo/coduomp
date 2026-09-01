#include "fx_classes.hpp"

/* The Windows vtables at 0x005a1fc4..0x005a20dc prove each class binding;
 * the same-module Mac symbols supply the original class names. */

/* Source: CoDUOMP.exe 0x004a2770..0x004a2772. */
int32_t CLine::TypeID()
{
    return FX_EFFECT_TYPE_LINE;
}

/* Source: CoDUOMP.exe 0x004a2e10..0x004a2e12. */
int32_t CTail::TypeID()
{
    return FX_EFFECT_TYPE_TAIL;
}

/* Source: CoDUOMP.exe 0x004a30a0..0x004a30a2. */
int32_t CCylinder::TypeID()
{
    return FX_EFFECT_TYPE_CYLINDER;
}

/* Source: CoDUOMP.exe 0x004a3b40..0x004a3b42. */
int32_t CEmitter::TypeID()
{
    return FX_EFFECT_TYPE_EMITTER;
}

/* Source: CoDUOMP.exe 0x004a25b0..0x004a25b2. */
int32_t COrientedParticle::TypeID()
{
    return FX_EFFECT_TYPE_ORIENTED_PARTICLE;
}

/* Source: CoDUOMP.exe 0x004a29b0..0x004a29b2. */
int32_t CElectricity::TypeID()
{
    return FX_EFFECT_TYPE_ELECTRICITY;
}

/* Source: CoDUOMP.exe 0x004a40b0..0x004a40b2. */
int32_t CLight::TypeID()
{
    return FX_EFFECT_TYPE_LIGHT;
}

/* Source: CoDUOMP.exe 0x004a45e0..0x004a45e2.
 * Class binding: Windows vtable 0x005a2be8. */
int32_t CQuad::TypeID()
{
    return FX_EFFECT_TYPE_QUAD;
}

/* Source: CoDUOMP.exe 0x004a4ca0..0x004a4ca2. */
int32_t CDecal::TypeID()
{
    return FX_EFFECT_TYPE_DECAL;
}

/* Source: CoDUOMP.exe 0x004a4ff0..0x004a4ff2.
 * Name and class binding: Windows vtable 0x005a2bcc and same-module Mac symbol
 * CFlash::TypeID. */
int32_t CFlash::TypeID()
{
    return FX_EFFECT_TYPE_FLASH;
}

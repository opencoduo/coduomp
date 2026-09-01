#include "fx_classes.hpp"

/* The Windows pool descriptors at 0x0389ff80..0x0389ffe4 hold an element
 * count followed by the current block.  Their startup initializers prove that
 * the count is the usable block payload divided by the original i386 class
 * size.  The eleven compiler-emitted initializer ranges are 0x00584f60 (CEffect),
 * 0x00584f90 (CQuad),
 * 0x00584fc0 (CLight/CFlash), 0x00584ff0 (CParticle), 0x00585020 (CLine), 0x00585050
 * (CElectricity), 0x00585080 (COrientedParticle), 0x005850b0 (CTail),
 * 0x005850e0 (CCylinder), 0x00585110 (CEmitter), and 0x00585140 (CDecal).
 * Recomputing the quotient preserves the allocator design when native pointer
 * fields widen on 64-bit hosts. */
static constexpr size_t fxPoolPayloadBytes =
    sizeof(((fx_mem_block_t *)nullptr)->storage);

/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxEffectAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CEffect)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxParticleAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CParticle)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxOrientedParticleAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(COrientedParticle)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxLineAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CLine)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxElectricityAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CElectricity)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxTailAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CTail)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxCylinderAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CCylinder)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxEmitterAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CEmitter)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxQuadAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CQuad)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxLightAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CLight)), nullptr
};
/* PE_ZERO_WITH_RUNTIME_INITIALIZER: its startup thunk initializes it. */
fx_pool_allocator_t fxDecalAllocator = {
    static_cast<int32_t>(fxPoolPayloadBytes / sizeof(CDecal)), nullptr
};

/* These source-level class allocation functions are the twelve-byte Windows
 * adapters that select a class pool and call its compiler-instantiated fixed-
 * pool allocator. CFlash inherits CLight's allocator because both are 0x108
 * bytes and the original factory uses descriptor 0x0389ffb0 for both. */

/* Source: CoDUOMP.exe 0x004aed30..0x004aed3b. */
void *CQuad::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxQuadAllocator, size);
}

/* Source: CoDUOMP.exe 0x004aee40..0x004aee4b. */
void *CLight::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxLightAllocator, size);
}

/* Source: CoDUOMP.exe 0x004af120..0x004af12b. */
void *CParticle::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxParticleAllocator, size);
}

/* Source: CoDUOMP.exe 0x004af150..0x004af15b. */
void *CLine::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxLineAllocator, size);
}

/* Source: CoDUOMP.exe 0x004af170..0x004af17b. */
void *CElectricity::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxElectricityAllocator, size);
}

/* Source: CoDUOMP.exe 0x004af1a0..0x004af1ab. */
void *COrientedParticle::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxOrientedParticleAllocator, size);
}

/* Source: CoDUOMP.exe 0x004af1e0..0x004af1eb. */
void *CTail::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxTailAllocator, size);
}

/* Source: CoDUOMP.exe 0x004af240..0x004af24b. */
void *CCylinder::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxCylinderAllocator, size);
}

/* Source: CoDUOMP.exe 0x004af400..0x004af40b. */
void *CEmitter::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxEmitterAllocator, size);
}

/* Source: CoDUOMP.exe 0x004af470..0x004af47b. */
void *CDecal::operator new(size_t size) noexcept
{
    return coduomp_fx_mem_alloc_from_pool(&fxDecalAllocator, size);
}

/* These source-level class deallocation functions are the ten-byte Windows
 * adapters that select a class pool and tail-call its compiler-instantiated
 * fixed-pool free routine. */

/* Source: CoDUOMP.exe 0x004a0880..0x004a0889.
 * Descriptor: 0x0389ffd0 (CEffect). */
void CEffect::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxEffectAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004a08a0..0x004a08a9.
 * Descriptor: 0x0389ff98 (CParticle). */
void CParticle::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxParticleAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004a08d0..0x004a08d9.
 * Descriptor: 0x0389ffc0 (COrientedParticle). */
void COrientedParticle::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxOrientedParticleAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004a08b0..0x004a08b9.
 * Descriptor: 0x0389ffe0 (CLine). */
void CLine::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxLineAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004a08c0..0x004a08c9.
 * Descriptor: 0x0389ff88 (CElectricity). */
void CElectricity::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxElectricityAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004a08e0..0x004a08e9.
 * Descriptor: 0x0389ffc8 (CTail). */
void CTail::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxTailAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004a08f0..0x004a08f9.
 * Descriptor: 0x0389ffa0 (CCylinder). */
void CCylinder::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxCylinderAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004a0930..0x004a0939.
 * Descriptor: 0x0389ff80 (CEmitter). */
void CEmitter::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxEmitterAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004aed40..0x004aed49.
 * Descriptor: 0x0389ffd8 (CQuad). */
void CQuad::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxQuadAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004a0890..0x004a0899.
 * Descriptor: 0x0389ffb0 (CLight). */
void CLight::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxLightAllocator, allocation);
}

/* Source: CoDUOMP.exe 0x004a0940..0x004a0949.
 * Descriptor: 0x0389ffb8 (CDecal). */
void CDecal::operator delete(void *allocation) noexcept
{
    coduomp_fx_mem_free_from_pool(&fxDecalAllocator, allocation);
}

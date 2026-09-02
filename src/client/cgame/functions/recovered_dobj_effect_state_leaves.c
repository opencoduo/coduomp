// Complete DObj/effect/scoreboard leaves recovered from their exact instruction
// records. Address-shaped names remain where the original symbol is unproven.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

static const float CG_EFFECT_SECONDS_PER_MILLISECOND = 0.001f;

#if defined(__GNUC__) || defined(__clang__)
#define CG_EFFECT_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define CG_EFFECT_ALWAYS_INLINE inline
#endif

/* NOT_FROM_ORIGINAL_SOURCE: native instruction-width adapter for the original
 * FILD dword / FMUL m32 / FSTP m32 chain. This is not x87 emulation. */
static CG_EFFECT_ALWAYS_INLINE void
cgame_compat_effect_seconds(int32_t frameTime, float *secondsOut)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("fildl %1\n\t"
                         "fmuls %2\n\t"
                         "fstps %0"
                         : "=m"(*secondsOut)
                         : "m"(frameTime),
                           "m"(CG_EFFECT_SECONDS_PER_MILLISECOND)
                         : "st");
#else
    *secondsOut = (float)((long double)frameTime *
                          (long double)CG_EFFECT_SECONDS_PER_MILLISECOND);
#endif
}

#undef CG_EFFECT_ALWAYS_INLINE

void CG_ShutdownFlameChunks(void) /* 0x30027ab0 */
{
    (void)cgame_syscall(CG_Z_FREE_INTERNAL,
                        (intptr_t)cg_flameChunks);
    cg_flameChunks = 0;
}

// Source RVA: 0x3001fb80
void SetEffectFrameTime(int32_t effectContext)
{
    /* 0x3001fb81: FILD cg_frametime straight into FMUL 0.001f (0x3007bd94) with NO
     * intermediate float store, then one FSTP (0x3001fb8d). So cg_frametime is not
     * (float)-cast here -- unlike CG_AddPacketEntities' twin idiom, which DOES
     * FSTP/FLD cg_frametime first and therefore keeps its cast. */
    /* The narrow native adapter pins those operand/store widths even where a
     * host compiler would materialize the float constant as an m80 value. */
    float seconds;
    cgame_compat_effect_seconds(cg_frametime, &seconds);
    (void)cgame_syscall(CG_DOBJ_ADVANCE_SERVER_TIME, effectContext,
                        CG_FloatBits(seconds));
}

// Source RVA: 0x30016400
void CG_SetDObjInfo(int32_t entityNum, uint32_t key,
                    XModel *model)
{
    cg_dObjInfoKeys[entityNum] = key;
    cg_dObjInfoHandles[entityNum] = model;
}

// Source RVA: 0x30016410
qboolean CG_CheckDObjInfoMatches(int32_t entityNum, uint32_t key,
                                 const XModel *model)
{
    return cg_dObjInfoKeys[entityNum] == key &&
                   cg_dObjInfoHandles[entityNum] == model
               ? qtrue
               : qfalse;
}

// Source RVA: 0x30016440
void CG_FreeClientDObjInfo(int32_t entityNum)
{
    (void)cgame_syscall(CG_SAFE_CLIENT_DOBJ_FREE, entityNum, 1);
    cg_dObjInfoKeys[entityNum] = 0;
    cg_dObjInfoHandles[entityNum] = 0;
}

// Source RVA: 0x300357d0
int32_t GetEntityTypeIfModelLoaded(int32_t entityNum)
{
    centity_t *entity = cgame_compat_unchecked_cgentity(entityNum);
    if (entity->currentValid == 0)
        return -1;
    return entity->currentState.eType;
}

// Source RVA: 0x30037e30
void CG_ScrollScoreboardUp(void)
{
    if (cg_scoreboardScrollPos > 0) {
        cg_scoreboardScrollPos = coduo_int32_from_bits(
            (uint32_t)cg_scoreboardScrollPos -
            (uint32_t)cg_scoreboardScrollStep_vmCvar.integer);
        if (cg_scoreboardScrollPos < 0)
            cg_scoreboardScrollPos = 0;
    }
}

const vec4_t *CG_ColorForTeam(int32_t team) /* 0x3001dc90 */
{
    switch (team) {
    case 1:
        return &cg_hudColorTable[0];
    case 2:
        return &cg_hudColorTable[1];
    case 3:
        return &cg_hudColorTable[3];
    default:
        return &cg_hudColorTable[2];
    }
}

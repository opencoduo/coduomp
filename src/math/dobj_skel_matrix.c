#include "q_math.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#include "compat/coduo_x87emu.h"

/*
 * DObjSkelMatrixMultiply43 composes a padded 0x40-byte DObj skeleton matrix
 * with a compact 0x30-byte affine matrix and writes a compact matrix.  The
 * four authoritative Windows bodies are byte-identical:
 *
 *   CoDUOMP.exe                 0x004328a0
 *   uo_cgame_mp_x86.dll        0x3004aa00
 *   uo_ui_mp_x86.dll           0x400029d0
 *   uo_game_mp_x86.dll         0x20017a50
 *
 * The Linux engine body at 0x080682e5 is byte-identical to the Linux game
 * module symbol at RVA 0x0003bef1.  Both platform families implement the same
 * affine composition and column-major store order, but Windows retains an
 * irregular compiler-selected term order under PC=53 while Linux folds every
 * dot in X,Y,Z order under PC=64.  The supporting PowerPC game_mp.dll body at
 * file offset 0x00016810 has the same mixed matrix layouts, composition, and
 * store order, expressed with PPC fused multiply-add instructions.  Preserve
 * complete authoritative platform bodies.
 */

#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
#define DOBJ_M43_STRINGIFY_INNER(value) #value
#define DOBJ_M43_STRINGIFY(value) DOBJ_M43_STRINGIFY_INNER(value)
#define DOBJ_M43_ADDRESS(base, offset)                                      \
    DOBJ_M43_STRINGIFY(offset) "(%[" DOBJ_M43_STRINGIFY(base) "])"
#define DOBJ_M43_NATIVE_DOT3(destinationOffset,                              \
                            aBase, aOffset, bBase, bOffset,                  \
                            cBase, cOffset, dBase, dOffset,                  \
                            eBase, eOffset, fBase, fOffset)                  \
    __asm__ __volatile__("flds " DOBJ_M43_ADDRESS(aBase, aOffset) "\n\t" \
                         "fmuls " DOBJ_M43_ADDRESS(bBase, bOffset) "\n\t" \
                         "flds " DOBJ_M43_ADDRESS(cBase, cOffset) "\n\t" \
                         "fmuls " DOBJ_M43_ADDRESS(dBase, dOffset) "\n\t" \
                         "faddp %%st, %%st(1)\n\t"                       \
                         "flds " DOBJ_M43_ADDRESS(eBase, eOffset) "\n\t" \
                         "fmuls " DOBJ_M43_ADDRESS(fBase, fOffset) "\n\t" \
                         "faddp %%st, %%st(1)\n\t"                       \
                         "fstps " DOBJ_M43_ADDRESS(store,                   \
                                                    destinationOffset)       \
                         :                                                   \
                         : [left] "r"(left), [right] "r"(right),            \
                           [store] "r"(store)                               \
                         : "st", "st(1)", "memory")
#define DOBJ_M43_NATIVE_DOT4(destinationOffset,                              \
                            aBase, aOffset, bBase, bOffset,                  \
                            cBase, cOffset, dBase, dOffset,                  \
                            eBase, eOffset, fBase, fOffset, addOffset)       \
    __asm__ __volatile__("flds " DOBJ_M43_ADDRESS(aBase, aOffset) "\n\t" \
                         "fmuls " DOBJ_M43_ADDRESS(bBase, bOffset) "\n\t" \
                         "flds " DOBJ_M43_ADDRESS(cBase, cOffset) "\n\t" \
                         "fmuls " DOBJ_M43_ADDRESS(dBase, dOffset) "\n\t" \
                         "faddp %%st, %%st(1)\n\t"                       \
                         "flds " DOBJ_M43_ADDRESS(eBase, eOffset) "\n\t" \
                         "fmuls " DOBJ_M43_ADDRESS(fBase, fOffset) "\n\t" \
                         "faddp %%st, %%st(1)\n\t"                       \
                         "fadds " DOBJ_M43_ADDRESS(right, addOffset) "\n\t" \
                         "fstps " DOBJ_M43_ADDRESS(store,                   \
                                                    destinationOffset)       \
                         :                                                   \
                         : [left] "r"(left), [right] "r"(right),            \
                           [store] "r"(store)                               \
                         : "st", "st(1)", "memory")
#define DOBJ_M43_HAS_NATIVE_X87 1
#else
#define DOBJ_M43_HAS_NATIVE_X87 0
#endif

#if defined(WINDOWS_BEHAVIOR)

void DObjSkelMatrixMultiply43(const DObjSkelMat *left,
                              const matrix43_t *right, matrix43_t *output)
{
#if DOBJ_M43_HAS_NATIVE_X87
    float *store = &output->axis[0][0];

    DOBJ_M43_NATIVE_DOT3(0, left, 0, right, 0,
                         left, 4, right, 12, right, 24, left, 8);
    DOBJ_M43_NATIVE_DOT3(12, left, 20, right, 12,
                         left, 16, right, 0, left, 24, right, 24);
    DOBJ_M43_NATIVE_DOT3(24, left, 36, right, 12,
                         left, 32, right, 0, left, 40, right, 24);

    DOBJ_M43_NATIVE_DOT3(4, left, 4, right, 16,
                         right, 28, left, 8, right, 4, left, 0);
    DOBJ_M43_NATIVE_DOT3(16, left, 24, right, 28,
                         left, 20, right, 16, left, 16, right, 4);
    DOBJ_M43_NATIVE_DOT3(28, left, 40, right, 28,
                         left, 36, right, 16, left, 32, right, 4);

    DOBJ_M43_NATIVE_DOT3(8, left, 0, right, 8,
                         left, 4, right, 20, right, 32, left, 8);
    DOBJ_M43_NATIVE_DOT3(20, left, 24, right, 32,
                         left, 20, right, 20, left, 16, right, 8);
    DOBJ_M43_NATIVE_DOT3(32, left, 40, right, 32,
                         left, 36, right, 20, left, 32, right, 8);

    DOBJ_M43_NATIVE_DOT4(36, left, 52, right, 12,
                         left, 48, right, 0, left, 56, right, 24, 36);
    DOBJ_M43_NATIVE_DOT4(40, left, 56, right, 28,
                         left, 52, right, 16, left, 48, right, 4, 40);
    DOBJ_M43_NATIVE_DOT4(44, left, 56, right, 32,
                         left, 52, right, 20, left, 48, right, 8, 44);
#else
    output->axis[0][0] = (float)(
        ((double)left->axis[0][0] * right->axis[0][0] +
         (double)left->axis[0][1] * right->axis[1][0]) +
        (double)right->axis[2][0] * left->axis[0][2]);
    output->axis[1][0] = (float)(
        ((double)left->axis[1][1] * right->axis[1][0] +
         (double)left->axis[1][0] * right->axis[0][0]) +
        (double)left->axis[1][2] * right->axis[2][0]);
    output->axis[2][0] = (float)(
        ((double)left->axis[2][1] * right->axis[1][0] +
         (double)left->axis[2][0] * right->axis[0][0]) +
        (double)left->axis[2][2] * right->axis[2][0]);

    output->axis[0][1] = (float)(
        ((double)left->axis[0][1] * right->axis[1][1] +
         (double)right->axis[2][1] * left->axis[0][2]) +
        (double)right->axis[0][1] * left->axis[0][0]);
    output->axis[1][1] = (float)(
        ((double)left->axis[1][2] * right->axis[2][1] +
         (double)left->axis[1][1] * right->axis[1][1]) +
        (double)left->axis[1][0] * right->axis[0][1]);
    output->axis[2][1] = (float)(
        ((double)left->axis[2][2] * right->axis[2][1] +
         (double)left->axis[2][1] * right->axis[1][1]) +
        (double)left->axis[2][0] * right->axis[0][1]);

    output->axis[0][2] = (float)(
        ((double)left->axis[0][0] * right->axis[0][2] +
         (double)left->axis[0][1] * right->axis[1][2]) +
        (double)right->axis[2][2] * left->axis[0][2]);
    output->axis[1][2] = (float)(
        ((double)left->axis[1][2] * right->axis[2][2] +
         (double)left->axis[1][1] * right->axis[1][2]) +
        (double)left->axis[1][0] * right->axis[0][2]);
    output->axis[2][2] = (float)(
        ((double)left->axis[2][2] * right->axis[2][2] +
         (double)left->axis[2][1] * right->axis[1][2]) +
        (double)left->axis[2][0] * right->axis[0][2]);

    output->origin[0] = (float)(
        (((double)left->origin[1] * right->axis[1][0] +
          (double)left->origin[0] * right->axis[0][0]) +
         (double)left->origin[2] * right->axis[2][0]) + right->origin[0]);
    output->origin[1] = (float)(
        (((double)left->origin[2] * right->axis[2][1] +
          (double)left->origin[1] * right->axis[1][1]) +
         (double)left->origin[0] * right->axis[0][1]) + right->origin[1]);
    output->origin[2] = (float)(
        (((double)left->origin[2] * right->axis[2][2] +
          (double)left->origin[1] * right->axis[1][2]) +
         (double)left->origin[0] * right->axis[0][2]) + right->origin[2]);
#endif
}

/*
 * DObjSkel2MatrixMultiply43 composes a padded 0x40-byte DObj skeleton matrix
 * with a compact 0x30-byte affine matrix and writes a padded record, on every
 * platform.  The Windows bodies the world-tag/bone lookups actually call are
 * byte-identical over their complete 0x14a-byte extent (SHA-256
 * 02a097d103dfe6540225f27cb9bff507b031f537857a6371c7ebbed08ece119e):
 *
 *   CoDUOMP.exe                 0x004329d0
 *   uo_cgame_mp_x86.dll        0x3004ab30
 *   uo_ui_mp_x86.dll           0x40002b00
 *   uo_game_mp_x86.dll         0x20017b80
 *
 * uo_game_mp_x86.dll's only two calls of 0x20017b80 are
 * G_DObjGetWorldBoneIndexMatrix (0x20052920) and G_DObjGetWorldTagMatrix
 * (0x20052a10); the cgame/ui copies are reconstructed as CG_ComposeBoneMatrix.
 * A compact-row sibling (CoDUOMP.exe 0x00432640, uo_cgame_mp_x86.dll
 * 0x3004a7a0, uo_ui_mp_x86.dll 0x40002770, uo_game_mp_x86.dll 0x200177f0;
 * SHA-256 c644b145...) also exists byte-identical in all four binaries but has
 * ZERO callers in uo_game_mp_x86.dll; an earlier reconstruction adapted the
 * two game-module call sites to that dead body through a row-conversion shim,
 * which reversed the composition (entity-first instead of bone-first, with the
 * translation rotated by the inverse entity axis).
 *
 * The irregular term order below is 0x20017b80's.  Native x87 builds state
 * that order directly; the portable Windows path uses binary64 intermediates
 * to model the process PC=53 control-word policy.  The homogeneous 1.0f lane
 * is stored before the final origin-Z store, as in the original.
 */
void DObjSkel2MatrixMultiply43(const DObjSkelMat *left,
                               const matrix43_t *right,
                               DObjSkelMat *output)
{
#if DOBJ_M43_HAS_NATIVE_X87
    float *store = &output->axis[0][0];

    DOBJ_M43_NATIVE_DOT3(0, left, 0, right, 0,
                         left, 4, right, 12, right, 24, left, 8);
    DOBJ_M43_NATIVE_DOT3(16, left, 20, right, 12,
                         left, 16, right, 0, left, 24, right, 24);
    DOBJ_M43_NATIVE_DOT3(32, left, 36, right, 12,
                         left, 32, right, 0, left, 40, right, 24);

    DOBJ_M43_NATIVE_DOT3(4, left, 4, right, 16,
                         right, 28, left, 8, right, 4, left, 0);
    DOBJ_M43_NATIVE_DOT3(20, left, 24, right, 28,
                         left, 20, right, 16, left, 16, right, 4);
    DOBJ_M43_NATIVE_DOT3(36, left, 40, right, 28,
                         left, 36, right, 16, left, 32, right, 4);

    DOBJ_M43_NATIVE_DOT3(8, left, 0, right, 8,
                         left, 4, right, 20, right, 32, left, 8);
    DOBJ_M43_NATIVE_DOT3(24, left, 24, right, 32,
                         left, 20, right, 20, left, 16, right, 8);
    DOBJ_M43_NATIVE_DOT3(40, left, 40, right, 32,
                         left, 36, right, 20, left, 32, right, 8);

    output->axis[0][3] = 0.0f;
    output->axis[1][3] = 0.0f;
    output->axis[2][3] = 0.0f;

    DOBJ_M43_NATIVE_DOT4(48, left, 48, right, 0,
                         right, 24, left, 56, left, 52, right, 12, 36);
    DOBJ_M43_NATIVE_DOT4(52, right, 28, left, 56,
                         right, 16, left, 52, right, 4, left, 48, 40);
    output->origin[3] = 1.0f;
    DOBJ_M43_NATIVE_DOT4(56, left, 48, right, 8,
                         left, 56, right, 32, left, 52, right, 20, 44);
#else
    output->axis[0][0] = (float)(
        ((double)left->axis[0][0] * right->axis[0][0] +
         (double)left->axis[0][1] * right->axis[1][0]) +
        (double)right->axis[2][0] * left->axis[0][2]);
    output->axis[1][0] = (float)(
        ((double)left->axis[1][1] * right->axis[1][0] +
         (double)left->axis[1][0] * right->axis[0][0]) +
        (double)left->axis[1][2] * right->axis[2][0]);
    output->axis[2][0] = (float)(
        ((double)left->axis[2][1] * right->axis[1][0] +
         (double)left->axis[2][0] * right->axis[0][0]) +
        (double)left->axis[2][2] * right->axis[2][0]);

    output->axis[0][1] = (float)(
        ((double)left->axis[0][1] * right->axis[1][1] +
         (double)right->axis[2][1] * left->axis[0][2]) +
        (double)right->axis[0][1] * left->axis[0][0]);
    output->axis[1][1] = (float)(
        ((double)left->axis[1][2] * right->axis[2][1] +
         (double)left->axis[1][1] * right->axis[1][1]) +
        (double)left->axis[1][0] * right->axis[0][1]);
    output->axis[2][1] = (float)(
        ((double)left->axis[2][2] * right->axis[2][1] +
         (double)left->axis[2][1] * right->axis[1][1]) +
        (double)left->axis[2][0] * right->axis[0][1]);

    output->axis[0][2] = (float)(
        ((double)left->axis[0][0] * right->axis[0][2] +
         (double)left->axis[0][1] * right->axis[1][2]) +
        (double)right->axis[2][2] * left->axis[0][2]);
    output->axis[1][2] = (float)(
        ((double)left->axis[1][2] * right->axis[2][2] +
         (double)left->axis[1][1] * right->axis[1][2]) +
        (double)left->axis[1][0] * right->axis[0][2]);
    output->axis[2][2] = (float)(
        ((double)left->axis[2][2] * right->axis[2][2] +
         (double)left->axis[2][1] * right->axis[1][2]) +
        (double)left->axis[2][0] * right->axis[0][2]);

    output->axis[0][3] = 0.0f;
    output->axis[1][3] = 0.0f;
    output->axis[2][3] = 0.0f;

    output->origin[0] = (float)(
        (((double)left->origin[0] * right->axis[0][0] +
          (double)right->axis[2][0] * left->origin[2]) +
         (double)left->origin[1] * right->axis[1][0]) +
        right->origin[0]);
    output->origin[1] = (float)(
        (((double)right->axis[2][1] * left->origin[2] +
          (double)right->axis[1][1] * left->origin[1]) +
         (double)right->axis[0][1] * left->origin[0]) +
        right->origin[1]);

    const double originZ =
        (((double)left->origin[0] * right->axis[0][2] +
          (double)left->origin[2] * right->axis[2][2]) +
         (double)left->origin[1] * right->axis[1][2]) +
        right->origin[2];
    output->origin[3] = 1.0f;
    output->origin[2] = (float)originZ;
#endif
}

#else

#if EMULATE_X87
#define DOBJ_M43_LINUX_DOT3(destination, a, b, c, d, e, f)                   \
    ((destination) = x87f_store_f32(x87f_add(                               \
         x87f_add(x87f_mul(x87f_load_f32(a), x87f_load_f32(b)),             \
                  x87f_mul(x87f_load_f32(c), x87f_load_f32(d))),            \
         x87f_mul(x87f_load_f32(e), x87f_load_f32(f)))))
#define DOBJ_M43_LINUX_DOT4(destination, a, b, c, d, e, f, g)                \
    ((destination) = x87f_store_f32(x87f_add(                               \
         x87f_add(                                                          \
             x87f_add(x87f_mul(x87f_load_f32(a), x87f_load_f32(b)),         \
                      x87f_mul(x87f_load_f32(c), x87f_load_f32(d))),        \
             x87f_mul(x87f_load_f32(e), x87f_load_f32(f))),                \
         x87f_load_f32(g))))
#else
#define DOBJ_M43_LINUX_DOT3(destination, a, b, c, d, e, f)                   \
    ((destination) = (float)(                                               \
         ((long double)(a) * (long double)(b) +                             \
          (long double)(c) * (long double)(d)) +                            \
         (long double)(e) * (long double)(f)))
#define DOBJ_M43_LINUX_DOT4(destination, a, b, c, d, e, f, g)                \
    ((destination) = (float)(                                               \
         (((long double)(a) * (long double)(b) +                            \
           (long double)(c) * (long double)(d)) +                           \
          (long double)(e) * (long double)(f)) +                            \
         (long double)(g)))
#endif

void DObjSkelMatrixMultiply43(const DObjSkelMat *left,
                              const matrix43_t *right, matrix43_t *output)
{
#if DOBJ_M43_HAS_NATIVE_X87 && !EMULATE_X87
    float *store = &output->axis[0][0];

    DOBJ_M43_NATIVE_DOT3(0, left, 0, right, 0,
                         left, 4, right, 12, left, 8, right, 24);
    DOBJ_M43_NATIVE_DOT3(12, left, 16, right, 0,
                         left, 20, right, 12, left, 24, right, 24);
    DOBJ_M43_NATIVE_DOT3(24, left, 32, right, 0,
                         left, 36, right, 12, left, 40, right, 24);

    DOBJ_M43_NATIVE_DOT3(4, left, 0, right, 4,
                         left, 4, right, 16, left, 8, right, 28);
    DOBJ_M43_NATIVE_DOT3(16, left, 16, right, 4,
                         left, 20, right, 16, left, 24, right, 28);
    DOBJ_M43_NATIVE_DOT3(28, left, 32, right, 4,
                         left, 36, right, 16, left, 40, right, 28);

    DOBJ_M43_NATIVE_DOT3(8, left, 0, right, 8,
                         left, 4, right, 20, left, 8, right, 32);
    DOBJ_M43_NATIVE_DOT3(20, left, 16, right, 8,
                         left, 20, right, 20, left, 24, right, 32);
    DOBJ_M43_NATIVE_DOT3(32, left, 32, right, 8,
                         left, 36, right, 20, left, 40, right, 32);

    DOBJ_M43_NATIVE_DOT4(36, left, 48, right, 0,
                         left, 52, right, 12, left, 56, right, 24, 36);
    DOBJ_M43_NATIVE_DOT4(40, left, 48, right, 4,
                         left, 52, right, 16, left, 56, right, 28, 40);
    DOBJ_M43_NATIVE_DOT4(44, left, 48, right, 8,
                         left, 52, right, 20, left, 56, right, 32, 44);
#else
    DOBJ_M43_LINUX_DOT3(output->axis[0][0],
        left->axis[0][0], right->axis[0][0],
        left->axis[0][1], right->axis[1][0],
        left->axis[0][2], right->axis[2][0]);
    DOBJ_M43_LINUX_DOT3(output->axis[1][0],
        left->axis[1][0], right->axis[0][0],
        left->axis[1][1], right->axis[1][0],
        left->axis[1][2], right->axis[2][0]);
    DOBJ_M43_LINUX_DOT3(output->axis[2][0],
        left->axis[2][0], right->axis[0][0],
        left->axis[2][1], right->axis[1][0],
        left->axis[2][2], right->axis[2][0]);

    DOBJ_M43_LINUX_DOT3(output->axis[0][1],
        left->axis[0][0], right->axis[0][1],
        left->axis[0][1], right->axis[1][1],
        left->axis[0][2], right->axis[2][1]);
    DOBJ_M43_LINUX_DOT3(output->axis[1][1],
        left->axis[1][0], right->axis[0][1],
        left->axis[1][1], right->axis[1][1],
        left->axis[1][2], right->axis[2][1]);
    DOBJ_M43_LINUX_DOT3(output->axis[2][1],
        left->axis[2][0], right->axis[0][1],
        left->axis[2][1], right->axis[1][1],
        left->axis[2][2], right->axis[2][1]);

    DOBJ_M43_LINUX_DOT3(output->axis[0][2],
        left->axis[0][0], right->axis[0][2],
        left->axis[0][1], right->axis[1][2],
        left->axis[0][2], right->axis[2][2]);
    DOBJ_M43_LINUX_DOT3(output->axis[1][2],
        left->axis[1][0], right->axis[0][2],
        left->axis[1][1], right->axis[1][2],
        left->axis[1][2], right->axis[2][2]);
    DOBJ_M43_LINUX_DOT3(output->axis[2][2],
        left->axis[2][0], right->axis[0][2],
        left->axis[2][1], right->axis[1][2],
        left->axis[2][2], right->axis[2][2]);

    DOBJ_M43_LINUX_DOT4(output->origin[0],
        left->origin[0], right->axis[0][0],
        left->origin[1], right->axis[1][0],
        left->origin[2], right->axis[2][0], right->origin[0]);
    DOBJ_M43_LINUX_DOT4(output->origin[1],
        left->origin[0], right->axis[0][1],
        left->origin[1], right->axis[1][1],
        left->origin[2], right->axis[2][1], right->origin[1]);
    DOBJ_M43_LINUX_DOT4(output->origin[2],
        left->origin[0], right->axis[0][2],
        left->origin[1], right->axis[1][2],
        left->origin[2], right->axis[2][2], right->origin[2]);
#endif
}

/*
 * The Linux engine body at 0x08068578 and game-module body at RVA 0x0003c184
 * are byte-identical over all 677 bytes (SHA-256
 * 281466b61a2841604a792fcaedc88081309565c512a7ea23e04bb148c3ab0218).
 * Unlike Windows, their left/output operands are padded DObjSkelMat records;
 * they explicitly write the three zero padding lanes and homogeneous 1.0f.
 */
void DObjSkel2MatrixMultiply43(const DObjSkelMat *left,
                               const matrix43_t *right,
                               DObjSkelMat *output)
{
#if DOBJ_M43_HAS_NATIVE_X87 && !EMULATE_X87
    float *store = &output->axis[0][0];

    DOBJ_M43_NATIVE_DOT3(0, left, 0, right, 0,
                         left, 4, right, 12, left, 8, right, 24);
    DOBJ_M43_NATIVE_DOT3(16, left, 16, right, 0,
                         left, 20, right, 12, left, 24, right, 24);
    DOBJ_M43_NATIVE_DOT3(32, left, 32, right, 0,
                         left, 36, right, 12, left, 40, right, 24);

    DOBJ_M43_NATIVE_DOT3(4, left, 0, right, 4,
                         left, 4, right, 16, left, 8, right, 28);
    DOBJ_M43_NATIVE_DOT3(20, left, 16, right, 4,
                         left, 20, right, 16, left, 24, right, 28);
    DOBJ_M43_NATIVE_DOT3(36, left, 32, right, 4,
                         left, 36, right, 16, left, 40, right, 28);

    DOBJ_M43_NATIVE_DOT3(8, left, 0, right, 8,
                         left, 4, right, 20, left, 8, right, 32);
    DOBJ_M43_NATIVE_DOT3(24, left, 16, right, 8,
                         left, 20, right, 20, left, 24, right, 32);
    DOBJ_M43_NATIVE_DOT3(40, left, 32, right, 8,
                         left, 36, right, 20, left, 40, right, 32);

    output->axis[0][3] = 0.0f;
    output->axis[1][3] = 0.0f;
    output->axis[2][3] = 0.0f;

    DOBJ_M43_NATIVE_DOT4(48, left, 48, right, 0,
                         left, 52, right, 12, left, 56, right, 24, 36);
    DOBJ_M43_NATIVE_DOT4(52, left, 48, right, 4,
                         left, 52, right, 16, left, 56, right, 28, 40);
    DOBJ_M43_NATIVE_DOT4(56, left, 48, right, 8,
                         left, 52, right, 20, left, 56, right, 32, 44);
    output->origin[3] = 1.0f;
#else
    DOBJ_M43_LINUX_DOT3(output->axis[0][0],
        left->axis[0][0], right->axis[0][0],
        left->axis[0][1], right->axis[1][0],
        left->axis[0][2], right->axis[2][0]);
    DOBJ_M43_LINUX_DOT3(output->axis[1][0],
        left->axis[1][0], right->axis[0][0],
        left->axis[1][1], right->axis[1][0],
        left->axis[1][2], right->axis[2][0]);
    DOBJ_M43_LINUX_DOT3(output->axis[2][0],
        left->axis[2][0], right->axis[0][0],
        left->axis[2][1], right->axis[1][0],
        left->axis[2][2], right->axis[2][0]);

    DOBJ_M43_LINUX_DOT3(output->axis[0][1],
        left->axis[0][0], right->axis[0][1],
        left->axis[0][1], right->axis[1][1],
        left->axis[0][2], right->axis[2][1]);
    DOBJ_M43_LINUX_DOT3(output->axis[1][1],
        left->axis[1][0], right->axis[0][1],
        left->axis[1][1], right->axis[1][1],
        left->axis[1][2], right->axis[2][1]);
    DOBJ_M43_LINUX_DOT3(output->axis[2][1],
        left->axis[2][0], right->axis[0][1],
        left->axis[2][1], right->axis[1][1],
        left->axis[2][2], right->axis[2][1]);

    DOBJ_M43_LINUX_DOT3(output->axis[0][2],
        left->axis[0][0], right->axis[0][2],
        left->axis[0][1], right->axis[1][2],
        left->axis[0][2], right->axis[2][2]);
    DOBJ_M43_LINUX_DOT3(output->axis[1][2],
        left->axis[1][0], right->axis[0][2],
        left->axis[1][1], right->axis[1][2],
        left->axis[1][2], right->axis[2][2]);
    DOBJ_M43_LINUX_DOT3(output->axis[2][2],
        left->axis[2][0], right->axis[0][2],
        left->axis[2][1], right->axis[1][2],
        left->axis[2][2], right->axis[2][2]);

    output->axis[0][3] = 0.0f;
    output->axis[1][3] = 0.0f;
    output->axis[2][3] = 0.0f;

    DOBJ_M43_LINUX_DOT4(output->origin[0],
        left->origin[0], right->axis[0][0],
        left->origin[1], right->axis[1][0],
        left->origin[2], right->axis[2][0], right->origin[0]);
    DOBJ_M43_LINUX_DOT4(output->origin[1],
        left->origin[0], right->axis[0][1],
        left->origin[1], right->axis[1][1],
        left->origin[2], right->axis[2][1], right->origin[1]);
    DOBJ_M43_LINUX_DOT4(output->origin[2],
        left->origin[0], right->axis[0][2],
        left->origin[1], right->axis[1][2],
        left->origin[2], right->axis[2][2], right->origin[2]);
    output->origin[3] = 1.0f;
#endif
}

#undef DOBJ_M43_LINUX_DOT4
#undef DOBJ_M43_LINUX_DOT3
#endif

#if DOBJ_M43_HAS_NATIVE_X87
#undef DOBJ_M43_NATIVE_DOT4
#undef DOBJ_M43_NATIVE_DOT3
#undef DOBJ_M43_ADDRESS
#undef DOBJ_M43_STRINGIFY
#undef DOBJ_M43_STRINGIFY_INNER
#endif
#undef DOBJ_M43_HAS_NATIVE_X87

/*
 * Source reconstruction for player movement physics.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include "recovered_game.h"
#include "bg/bg_pmove.h"

/* ------------------------------------------------------------------ */
/*  Global pmove state                                                 */
/* ------------------------------------------------------------------ */

/* .rodata 0x9b4a4 / 0x9b4d4 (dynsym): noclip/UFO friction parameters. */
const float pm_stopspeed = 100.0f;
const float pm_friction = 5.5f;
const float pm_accelerate = 9.0f;
const float pm_waterSwimScale = 0.5f;
const float pm_waterWadeScale = 0.7f;
const float pm_prone_accelerate = 19.0f;
const float pm_ducked_accelerate = 12.0f;
const float pm_wateraccelerate = 4.0f;
const float pm_waterfriction = 1.0f;
const float pm_spectatorfriction = 5.0f;
const float pm_shellshockScale = 0.4f;

const vec4_t colorBlack = {0.0f, 0.0f, 0.0f, 1.0f};
const vec4_t colorRed = {1.0f, 0.0f, 0.0f, 1.0f};
const vec4_t colorGreen = {0.0f, 1.0f, 0.0f, 1.0f};
const vec4_t colorLtGreen = {0.0f, 0.7f, 0.0f, 1.0f};
const vec4_t colorBlue = {0.0f, 0.0f, 1.0f, 1.0f};
const vec4_t colorYellow = {1.0f, 1.0f, 0.0f, 1.0f};
const vec4_t colorLtYellow = {0.75f, 0.75f, 0.0f, 1.0f};
const vec4_t colorMdYellow = {0.5f, 0.5f, 0.0f, 1.0f};
const vec4_t colorMagenta = {1.0f, 0.0f, 1.0f, 1.0f};
const vec4_t colorCyan = {0.0f, 1.0f, 1.0f, 1.0f};
const vec4_t colorLtCyan = {0.0f, 0.75f, 0.75f, 1.0f};
const vec4_t colorMdCyan = {0.0f, 0.5f, 0.5f, 1.0f};
const vec4_t colorDkCyan = {0.0f, 0.25f, 0.25f, 1.0f};
const vec4_t colorWhite = {1.0f, 1.0f, 1.0f, 1.0f};
const vec4_t colorLtGrey = {0.75f, 0.75f, 0.75f, 1.0f};
const vec4_t colorMdGrey = {0.5f, 0.5f, 0.5f, 1.0f};
const vec4_t colorDkGrey = {0.25f, 0.25f, 0.25f, 1.0f};
const vec4_t colorOrange = {1.0f, 0.7f, 0.0f, 1.0f};
const vec4_t colorLtOrange = {0.75f, 0.525f, 0.0f, 1.0f};

/*  Target functions                                                   */
/* ================================================================== */

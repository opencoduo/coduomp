#include "bg_pmove.h"

/*
 * The Windows cgame/game tables agree byte-for-byte with the four exported
 * Linux game tables at 0x000a2640..0x000a2840. The Linux ELF spellings are the
 * surviving original symbol names and therefore own the shared declarations.
 */
const pmLerpEntry_t pmViewHeightLerpCrouchedRising[9] = {{0, 60.0f, 0},  {1, 59.5f, 0},   {4, 58.5f, 0},
                                                         {30, 56.0f, 0}, {80, 44.0f, 0},  {90, 41.5f, 0},
                                                         {95, 40.5f, 0}, {100, 40.0f, 0}, {PM_LERP_TABLE_END, 0.0f, 0}};

const pmLerpEntry_t pmViewHeightLerpStanding[9] = {{0, 40.0f, 0},  {5, 40.5f, 0},   {10, 41.5f, 0},
                                                   {20, 44.0f, 0}, {70, 56.0f, 0},  {96, 58.5f, 0},
                                                   {99, 59.5f, 0}, {100, 60.0f, 0}, {PM_LERP_TABLE_END, 0.0f, 0}};

const pmLerpEntry_t pmViewHeightLerpProne[11] = {{0, 40.0f, 0},
                                                 {11, 38.0f, 0},
                                                 {22, 33.0f, 0},
                                                 {34, 25.0f, 0},
                                                 {45, 16.0f, 0},
                                                 {50, 15.0f, 0},
                                                 {55, 16.0f, 0},
                                                 {70, 18.0f, 0},
                                                 {90, 17.0f, 0},
                                                 {100, 11.0f, 0},
                                                 {PM_LERP_TABLE_END, 0.0f, 0}};

const pmLerpEntry_t pmViewHeightLerpCrouchedFalling[8] = {{0, 11.0f, 0},  {5, 10.0f, 0},  {30, 21.0f, 0},  {50, 25.0f, 0},
                                                          {67, 31.0f, 0}, {83, 34.0f, 0}, {100, 40.0f, 0}, {PM_LERP_TABLE_END, 0.0f, 0}};

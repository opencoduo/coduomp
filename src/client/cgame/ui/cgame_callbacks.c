// Source: uo_cgame_mp_x86.dll display-context callbacks at the RVAs shown.
// Evidence: corresponding cgame_mp/mcode/uo_cgame_mp_x86/FUN_*.mcode records.

#include <stdint.h>
#include <stddef.h>

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

_Static_assert(offsetof(clientInfo_t, infoValid) == 0x00, "clientInfo_t.infoValid +0x00");
_Static_assert(offsetof(clientInfo_t, name) == 0x0c, "clientInfo_t.name +0x0c");
_Static_assert(offsetof(clientInfo_t, team) == 0x2c, "clientInfo_t.team +0x2c");

int32_t CG_FeederCount(float feederID) /* 0x3002d480 */
{
    enum {
        CG_FEEDER_AXIS_PLAYERS = 5,
        CG_FEEDER_ALLIES_PLAYERS = 6,
        CG_FEEDER_ALL_PLAYERS = 11
    };

    int32_t count = 0;
    if (feederID == CG_FEEDER_AXIS_PLAYERS || feederID == CG_FEEDER_ALLIES_PLAYERS) {
        team_t team = feederID == CG_FEEDER_AXIS_PLAYERS ? TEAM_AXIS : TEAM_ALLIES;
        for (int32_t i = 0; i < cg_scoreboardNumClients; ++i)
            if (cg_scoreboardEntries[i].team == team)
                ++count;
        return count;
    }
    return feederID == CG_FEEDER_ALL_PLAYERS ? cg_scoreboardNumClients : 0;
}

qboolean CG_OwnerDrawVisible(int32_t flags) /* 0x300318d0 */
{
    enum {
        OWNER_VISIBLE_HEALTH_BELOW_25 = 0x80,
        OWNER_VISIBLE_HEALTH_ABOVE_25 = 0x4000,
        OWNER_VISIBLE_SELECTED_IS_END = 0x8000,
        OWNER_VISIBLE_SELECTED_NOT_END = 0x10000,
        OWNER_VISIBLE_HEALTH_THRESHOLD = 25,
    };

    if (((uint32_t)flags & OWNER_VISIBLE_SELECTED_IS_END) != 0) {
        const int32_t selected = cg_currentSelectedPlayer_vmCvar.integer;
        const int32_t count = cg_hudEmitCount;
        return selected == count;
    }
    if (((uint32_t)flags & OWNER_VISIBLE_SELECTED_NOT_END) != 0) {
        const int32_t selected = cg_currentSelectedPlayer_vmCvar.integer;
        const int32_t count = cg_hudEmitCount;
        return selected != count;
    }

    /* Retail loads the snapshot pointer here but only dereferences health in
     * a branch whose corresponding visibility bit is set. Keep the two field
     * reads branch-local; an unconditional cached read changes the no-health-
     * flag path and also erases the second load when both bits are present. */
    const snapshot_t *snap = cg_snap;
    if (((uint32_t)flags & OWNER_VISIBLE_HEALTH_BELOW_25) != 0) {
        if (snap->ps.stats[STAT_HEALTH] < OWNER_VISIBLE_HEALTH_THRESHOLD)
            return qtrue;
    }
    if (((uint32_t)flags & OWNER_VISIBLE_HEALTH_ABOVE_25) != 0) {
        if (snap->ps.stats[STAT_HEALTH] > OWNER_VISIBLE_HEALTH_THRESHOLD)
            return qtrue;
    }
    return qfalse;
}

int32_t CG_ClientNumFromName(const char *name) /* 0x300327f0 */
{
    enum {
        CG_CLIENT_NAME_COMPARE_LIMIT = 99999
    };
    /* 0x300327f1 snapshots cgs_maxclients once in EBP for the whole loop. */
    const int32_t maxclients = cgs_maxclients;

    for (int32_t i = 0; i < maxclients; ++i) {
        /* The retail loop trusts the serverinfo-derived maxclients contract and
         * advances a raw 0x4d0 row pointer; it has no local array-bound branch. */
        const clientInfo_t *state = cgame_compat_unchecked_clientinfo(&bgs.clientinfo[0], i);
        const char *rowName = state->name;
        if (state->infoValid != 0 && rowName != NULL && name != NULL && Q_stricmpn(name, rowName, CG_CLIENT_NAME_COMPARE_LIMIT) == 0)
            return i;
    }
    return -1;
}

void CG_GetTeamColor(vec4_t color) /* 0x30032890 */
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    const int32_t clientNum = cg_snap->ps.psClientNum;
    if ((uint32_t)clientNum >= (uint32_t)MAX_CLIENTS) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CG_GetTeamColor: invalid client number %i",
                  clientNum);
        return;
    }
    const clientInfo_t *state = &bgs.clientinfo[clientNum];
    const int32_t infoValid = state->infoValid;
    if (infoValid != 0) {
        const int32_t team = state->team;
        if (team == TEAM_AXIS) {
            color[0] = 1.0f;
            color[1] = 0.0f;
            color[2] = 0.0f;
            color[3] = 0.25f;
            return;
        }
        if (team == TEAM_ALLIES) {
            /* 0x300328d9..0x300328e5 publishes green, red, blue, alpha. */
            color[1] = 0.0f;
            color[0] = 0.0f;
            color[2] = 1.0f;
            color[3] = 0.25f;
            return;
        }
    }
    color[0] = 0.0f;
    color[1] = 0.1700000018f;
    color[2] = 0.0f;
    color[3] = 0.25f;
}

/* 0x3001d270..0x3001d2ea.  The original register ABI receives value in ECX and
 * color in EAX.  The three x87 paths multiply by the literal float bit patterns
 * at 0x3007be1c (0x3d088889 = 1/30) and 0x3007be20 (0x3cf83e10 = 1/33). */
#define HUD_GREEN_STEP (0.033333335f)
#define HUD_BLUE_STEP (0.030303031f)

// Source RVA: 0x3001d270
void CG_HudColorForValue(int32_t value, vec4_t color)
{
    if (value <= 0) {
        color[2] = 0.0f;
        color[1] = 0.0f;
        color[0] = 0.0f;
        color[3] = 1.0f;
        return;
    }

    color[0] = 1.0f;
    color[3] = 1.0f;
    if (value >= 100) {
        color[2] = 1.0f;
    } else if (value < 66) {
        color[2] = 0.0f;
    } else {
        int32_t blueStep = value - 66;
        color[2] = (float)((long double)blueStep * (long double)HUD_BLUE_STEP); /* bare FILD @0x3001d2b0 */
    }

    if (value > 60) {
        color[1] = 1.0f;
    } else if (value < 30) {
        color[1] = 0.0f;
    } else {
        int32_t greenStep = value - 30;
        color[1] = (float)((long double)greenStep * (long double)HUD_GREEN_STEP); /* bare FILD @0x3001d2db */
    }
}

/* 0x3001d2f0..0x3001d376 is the display-context callback form of the same
 * source expression.  Its MOV chain proves the value is cg_snap+0x128. */
// Source RVA: 0x3001d2f0
void CG_GetHudEmitColor(vec4_t color)
{
    /* This is a second inline copy in the DLL, not a call to 0x3001d270. */
    int32_t value = cg_snap->ps.stats[STAT_HEALTH];

    if (value <= 0) {
        color[2] = 0.0f;
        color[1] = 0.0f;
        color[0] = 0.0f;
        color[3] = 1.0f;
        return;
    }

    color[0] = 1.0f;
    color[3] = 1.0f;
    if (value >= 100) {
        color[2] = 1.0f;
    } else if (value < 66) {
        color[2] = 0.0f;
    } else {
        int32_t blueStep = value - 66;
        color[2] = (float)((long double)blueStep * (long double)HUD_BLUE_STEP);
    }

    if (value > 60) {
        color[1] = 1.0f;
    } else if (value < 30) {
        color[1] = 0.0f;
    } else {
        int32_t greenStep = value - 30;
        color[1] = (float)((long double)greenStep * (long double)HUD_GREEN_STEP);
    }
}

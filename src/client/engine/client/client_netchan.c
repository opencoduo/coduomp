#include "cgame.h"

#include <stdint.h>

enum {
    CL_NETCHAN_ENCODE_START = 9,
    CL_NET_PROFILE_DRAW_X = 32,
    CL_NET_PROFILE_DRAW_START_Y = 90,
    CL_NET_PROFILE_DRAW_MODE = 0,
    CL_NET_PROFILE_CHAR_WIDTH = 8,
    CL_NET_PROFILE_CHAR_HEIGHT = 10,
    CL_NET_PROFILE_TEXT_STYLE = 0
};

/* Source: CoDUOMP.exe 0x004173f0..0x00417450.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004173f0_00417451.mcode.
 * Name and argument roles: exact same-module Mac symbol
 * CL_Netchan_Encode. */
void CL_Netchan_Encode(uint8_t *data, int32_t length)
{
    const char *const command =
        clc.serverCommands[
            clc.serverCommandSequence &
            (CODUO_RELIABLE_COMMAND_COUNT - 1)];
    uint8_t key = (uint8_t)cl.serverId ^
                  (uint8_t)clc.challenge ^
                  (uint8_t)clc.serverMessageSequence;
    int32_t commandIndex = 0;

    for (int32_t byteIndex = 0; byteIndex < length; ++byteIndex) {
        if (command[commandIndex] == '\0')
            commandIndex = 0;

        key ^= (uint8_t)(
            (uint8_t)command[commandIndex] << (byteIndex & 1));
        ++commandIndex;
        data[byteIndex] ^= key;
    }
}

/* Source: CoDUOMP.exe 0x00417460..0x004174bd.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00417460_004174be.mcode.
 * Name and argument roles: exact same-module Mac symbol
 * CL_Netchan_Decode. */
void CL_Netchan_Decode(uint8_t *data, int32_t length)
{
    const char *const command =
        clc.reliableCommands[
            clc.reliableAcknowledge &
            (CODUO_RELIABLE_COMMAND_COUNT - 1)];
    uint8_t key = (uint8_t)clc.challenge ^
                  (uint8_t)clc.serverMessageSequence;
    int32_t commandIndex = 0;

    for (int32_t byteIndex = 0; byteIndex < length; ++byteIndex) {
        if (command[commandIndex] == '\0')
            commandIndex = 0;

        key ^= (uint8_t)(
            (uint8_t)command[commandIndex] << (byteIndex & 1));
        ++commandIndex;
        data[byteIndex] ^= key;
    }
}

/* Source: CoDUOMP.exe 0x004174c0..0x004174c7.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004174c0_004174c8.mcode.
 * Name and signature: exact same-module Mac symbol
 * CL_Netchan_TransmitNextFragment. */
void CL_Netchan_TransmitNextFragment(netchan_t *channel)
{
    Netchan_TransmitNextFragment(channel);
}

/* Source: CoDUOMP.exe 0x004174d0..0x004174ec.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004174d0_004174ed.mcode.
 * Name and argument order: exact same-module Mac symbol
 * CL_Netchan_Transmit. */
void CL_Netchan_Transmit(netchan_t *channel, uint8_t *data,
                         int32_t length)
{
    CL_Netchan_Encode(data + CL_NETCHAN_ENCODE_START,
                      length - CL_NETCHAN_ENCODE_START);
    Netchan_Transmit(channel, length, data);
}

/* Source: CoDUOMP.exe 0x004174f0..0x0041751d.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004174f0_0041751e.mcode.
 * Name and signature: exact same-module Mac symbol
 * CL_Netchan_AddOOBProfilePacket. */
void CL_Netchan_AddOOBProfilePacket(int32_t length)
{
    if (net_profile->integer == 0)
        return;

    NetProf_PrepProfiling(&clc.netProfile);
    NetProf_AddPacket(&clc.netProfile->send, length, qfalse);
}

/* Source: CoDUOMP.exe 0x004175a0..0x004175e2.
 * Name: exact same-module Mac symbol CL_Netchan_UpdateProfileStats. */
void CL_Netchan_UpdateProfileStats(void)
{
    netProfileInfo_t *const netchanProfile = clc.netchan.profile;
    netProfileInfo_t *const oobProfile = clc.netProfile;

    if (netchanProfile != NULL) {
        NetProf_UpdateStatistics(&netchanProfile->send);
        NetProf_UpdateStatistics(&netchanProfile->receive);
    }
    if (oobProfile != NULL) {
        NetProf_UpdateStatistics(&oobProfile->send);
        NetProf_UpdateStatistics(&oobProfile->receive);
    }
}

/* Source: CoDUOMP.exe 0x004175f0..0x0041760f. Exact symbol is absent from the
 * Mac traceback table; its fixed VM draw arguments prove that it is the
 * network-profile line renderer which was inlined throughout the print body. */
static void CL_Netchan_DrawProfileString(int32_t y, const char *text)
{
    CL_DrawString(CL_NET_PROFILE_DRAW_X, y, text,
                  CL_NET_PROFILE_DRAW_MODE, CL_NET_PROFILE_CHAR_WIDTH,
                  CL_NET_PROFILE_CHAR_HEIGHT, CL_NET_PROFILE_TEXT_STYLE);
}

/* Source: CoDUOMP.exe 0x00417610..0x00417b98.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00417610_00417b99.mcode.
 * Name and printHeader argument: exact same-module Mac symbol
 * CL_Netchan_PrintProfileStats. Windows proves both profile sources, every
 * field offset, the aggregate-bps calculation, all strings, and the console
 * versus cgame-draw routing. */
void CL_Netchan_PrintProfileStats(qboolean printHeader)
{
    char line[MAX_STRING_CHARS];
    int32_t y = CL_NET_PROFILE_DRAW_START_Y;
    int32_t sentBytesPerSecond = 0;
    int32_t receivedBytesPerSecond = 0;
    netProfileInfo_t *const netchanProfile =
        clc.netchan.profile;
    netProfileInfo_t *const oobProfile = clc.netProfile;

    CL_Netchan_UpdateProfileStats();

    if (printHeader != qfalse)
        Com_Printf("\n\n");

    Com_sprintf(line, sizeof(line), "====================");
    if (printHeader != qfalse)
        Com_Printf("%s\n", line);
    else
        CL_Netchan_DrawProfileString(y, line);

    Com_sprintf(line, sizeof(line), "Client Network Profile:");
    if (printHeader != qfalse) {
        Com_Printf("%s\n\n", line);
    } else {
        y += 10;
        CL_Netchan_DrawProfileString(y, line);
        y += 10;
    }

    Com_sprintf(
        line, sizeof(line),
        "      Source    bps   max   min frag%%");
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        y += 10;
        CL_Netchan_DrawProfileString(y, line);
    }

    if (oobProfile != NULL) {
        sentBytesPerSecond =
            oobProfile->send.bytesPerSecond;
        receivedBytesPerSecond =
            oobProfile->receive.bytesPerSecond;
        Com_sprintf(
            line, sizeof(line),
            "    OOB Sent: %5i %5i %5i    -",
            oobProfile->send.bytesPerSecond,
            oobProfile->send.maxBytes,
            oobProfile->send.minBytes);
    } else {
        Com_sprintf(
            line, sizeof(line),
            "    OOB Sent:     0     0     0    -");
    }
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        y += 10;
        CL_Netchan_DrawProfileString(y, line);
    }

    if (oobProfile != NULL) {
        Com_sprintf(
            line, sizeof(line),
            "OOB Recieved: %5i %5i %5i    -",
            oobProfile->receive.bytesPerSecond,
            oobProfile->receive.maxBytes,
            oobProfile->receive.minBytes);
    } else {
        Com_sprintf(
            line, sizeof(line),
            "OOB Recieved:     0     0     0    -");
    }
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        y += 10;
        CL_Netchan_DrawProfileString(y, line);
    }

    if (netchanProfile != NULL) {
        sentBytesPerSecond +=
            netchanProfile->send.bytesPerSecond;
        receivedBytesPerSecond +=
            netchanProfile->receive.bytesPerSecond;
        Com_sprintf(
            line, sizeof(line),
            "        Sent: %5i %5i %5i  %3i%%",
            netchanProfile->send.bytesPerSecond,
            netchanProfile->send.maxBytes,
            netchanProfile->send.minBytes,
            netchanProfile->send.fragmentPercent);
    } else {
        Com_sprintf(
            line, sizeof(line),
            "        Sent:     0     0     0    0%%");
    }
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        y += 10;
        CL_Netchan_DrawProfileString(y, line);
    }

    if (netchanProfile != NULL) {
        Com_sprintf(
            line, sizeof(line),
            "    Recieved: %5i %5i %5i  %3i%%",
            netchanProfile->receive.bytesPerSecond,
            netchanProfile->receive.maxBytes,
            netchanProfile->receive.minBytes,
            netchanProfile->receive.fragmentPercent);
    } else {
        Com_sprintf(
            line, sizeof(line),
            "    Recieved:     0     0     0    0%%");
    }
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        y += 10;
        CL_Netchan_DrawProfileString(y, line);
    }

    Com_sprintf(
        line, sizeof(line), "       Total: %5i",
        sentBytesPerSecond + receivedBytesPerSecond);
    if (printHeader != qfalse) {
        Com_Printf("%s\n", line);
    } else {
        y += 10;
        CL_Netchan_DrawProfileString(y, line);
    }
}

#include <stdint.h>

#include "../module/ui_functions.h"
#include "../module/ui_globals.h"

enum {
    UI_DOWNLOAD_TEXT_LIMIT = 64,
    UI_DOWNLOAD_TEXT_STYLE = 3,
    UI_DOWNLOAD_MINIMUM_RATE_COUNT = 4096,
    UI_MILLISECONDS_PER_SECOND = 1000,
    UI_BYTES_PER_KILOBYTE = 1024,
    UI_DOWNLOAD_PERCENT_SCALE = 100
};

// Source: uo_ui_mp_x86.dll data 0x40040438..0x4004046f.
static char ui_downloadLabelDownloading[16] = "EXE_DOWNLOADING";
// 0x40040448
static char ui_downloadLabelEstimatedTime[20] = "EXE_EST_TIME_LEFT";
// 0x4004045c
static char ui_downloadLabelTransferRate[20] = "EXE_TRANS_RATE";

// Source: uo_ui_mp_x86.dll 0x40010700..0x40010e23
// Evidence: cgame_mp/mcode/uo_ui_mp_x86/FUN_40010700_40010e23.mcode
// Exact same-module PPC symbol: UI_DisplayDownloadInfo.
void UI_DisplayDownloadInfo(int32_t font, const char *downloadName, float centerPoint, float yStart, float scale)
{
    // 0x400325fc
    static const vec4_t labelColor = {0.75f, 0.75f, 0.75f, 1.0f};
    /* One stack color slot serves the background panels and is then
     * overwritten for the progress bars. */
    vec4_t fillColor;
    char downloadedSize[64];
    char totalSize[64];
    char timeLeft[64];
    char transferRate[64];
    const char *copiedText;
    int32_t downloadSize = (int32_t)trap_Cvar_VariableValue("cl_downloadSize");
    int32_t downloadCount = (int32_t)trap_Cvar_VariableValue("cl_downloadCount");
    int32_t downloadTime = (int32_t)trap_Cvar_VariableValue("cl_downloadTime");
    int32_t downloadPercentage = 0;
    int32_t progressWidth;
    int32_t index;

    fillColor[0] = 0.0f;
    fillColor[1] = 0.0f;
    fillColor[2] = 0.0f;
    fillColor[3] = 0.2f;
    UI_FillRect(0.0f, yStart + 184.0f, 640.0f, 85.0f, fillColor);
    UI_FillRect(0.0f, yStart + 185.0f, 640.0f, 83.0f, fillColor);
    UI_FillRect(0.0f, yStart + 186.0f, 640.0f, 81.0f, fillColor);

    if (downloadSize > 0) {
        /* The DLL divides the integers exactly (FILD 0x40010828, FIDIV
         * 0x40010844); (float) casts on both operands would round them,
         * losing precision once a download passes 2^24 bytes. */
        progressWidth = (int32_t)(((long double)downloadCount / downloadSize) * 640.0f);
        fillColor[0] = 1.0f;
        fillColor[1] = 0.0f;
        fillColor[2] = 0.0f;
        fillColor[3] = 0.15f;
        UI_FillRect(0.0f, yStart + 184.0f, (float)(progressWidth + 2), 85.0f, fillColor);
        UI_FillRect(0.0f, yStart + 185.0f, (float)(progressWidth + 1), 83.0f, fillColor);
        UI_FillRect(0.0f, yStart + 186.0f, (float)progressWidth, 81.0f, fillColor);
    }

    trap_R_SetColor(labelColor);
    trap_R_Text_Paint(24.0f, yStart + 210.0f, font, scale, labelColor, UI_SafeTranslateString(ui_downloadLabelDownloading), 0,
                      UI_DOWNLOAD_TEXT_LIMIT, UI_DOWNLOAD_TEXT_STYLE);
    trap_R_Text_Paint(24.0f, yStart + 235.0f, font, scale, labelColor, UI_SafeTranslateString(ui_downloadLabelEstimatedTime), 0,
                      UI_DOWNLOAD_TEXT_LIMIT, UI_DOWNLOAD_TEXT_STYLE);
    trap_R_Text_Paint(24.0f, yStart + 260.0f, font, scale, labelColor, UI_SafeTranslateString(ui_downloadLabelTransferRate), 0,
                      UI_DOWNLOAD_TEXT_LIMIT, UI_DOWNLOAD_TEXT_STYLE);

    if (downloadSize > 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        downloadPercentage = (int32_t)((int64_t)downloadCount * UI_DOWNLOAD_PERCENT_SCALE / downloadSize);
    }

    trap_R_Text_Paint(124.0f, yStart + 210.0f, font, scale, labelColor,
                      downloadSize > 0 ? va("%s (%d%%)", downloadName, downloadPercentage) : downloadName, 0, 0, UI_DOWNLOAD_TEXT_STYLE);

    UI_ReadableSize(downloadedSize, sizeof(downloadedSize), downloadCount);
    UI_ReadableSize(totalSize, sizeof(totalSize), downloadSize);

    if (downloadCount >= UI_DOWNLOAD_MINIMUM_RATE_COUNT && downloadTime != 0) {
        int32_t elapsedSeconds = (ui_displayContextStorage.context.realTime - downloadTime) / UI_MILLISECONDS_PER_SECOND;
        int32_t transferBytesPerSecond = 0;

        if (elapsedSeconds != 0) {
            transferBytesPerSecond = downloadCount / elapsedSeconds;
        }
        UI_ReadableSize(transferRate, sizeof(transferRate), transferBytesPerSecond);

        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (downloadSize >= UI_BYTES_PER_KILOBYTE && transferBytesPerSecond != 0) {
            int32_t totalSeconds = downloadSize / transferBytesPerSecond;
            int32_t downloadedKilobytes = downloadCount / UI_BYTES_PER_KILOBYTE;
            int32_t totalKilobytes = downloadSize / UI_BYTES_PER_KILOBYTE;
            int32_t remainingSeconds = totalSeconds - downloadedKilobytes * totalSeconds / totalKilobytes;
            int32_t estimateTotal = 0;
            const char *copiedLabel;
            const char *ofLabel;

            ui_downloadEstimates[ui_downloadEstimateIndex] = remainingSeconds;
            ++ui_downloadEstimateIndex;
            if (ui_downloadEstimateIndex >= UI_DOWNLOAD_ESTIMATE_SAMPLES) {
                ui_downloadEstimateIndex = 0;
            }
            for (index = 0; index < UI_DOWNLOAD_ESTIMATE_SAMPLES; ++index) {
                estimateTotal += ui_downloadEstimates[index];
            }
            UI_PrintTime(timeLeft, sizeof(timeLeft), estimateTotal / UI_DOWNLOAD_ESTIMATE_SAMPLES);
            trap_R_Text_Paint(159.0f, yStart + 235.0f, font, scale, labelColor, timeLeft, 0, 0, UI_DOWNLOAD_TEXT_STYLE);

            copiedLabel = UI_SafeTranslateString("EXE_COPIED");
            ofLabel = UI_SafeTranslateString("EXE_OF");
            copiedText = va("(%s %s %s %s)", downloadedSize, ofLabel, totalSize, copiedLabel);
            Text_PaintCenter(centerPoint, yStart + 320.0f, copiedText, scale, labelColor, font);
        } else {
            const char *copiedLabel;

            Text_PaintCenter(centerPoint, yStart + 235.0f, UI_SafeTranslateString("EXE_ESTIMATING"), scale, labelColor, font);
            copiedLabel = UI_SafeTranslateString("EXE_COPIED");
            if (downloadSize != 0) {
                copiedText = va("(%s %s %s %s)", downloadedSize, UI_SafeTranslateString("EXE_OF"), totalSize, copiedLabel);
            } else {
                copiedText = va("(%s %s)", downloadedSize, copiedLabel);
            }
            Text_PaintCenter(centerPoint, yStart + 320.0f, copiedText, scale, labelColor, font);
        }

        if (transferBytesPerSecond != 0) {
            trap_R_Text_Paint(124.0f, yStart + 260.0f, font, scale, labelColor,
                              va("%s/%s", transferRate, UI_SafeTranslateString("EXE_SECONDS")), 0, 0, UI_DOWNLOAD_TEXT_STYLE);
        }
    } else {
        const char *copiedLabel;

        Text_PaintCenter(centerPoint, yStart + 235.0f, UI_SafeTranslateString("EXE_ESTIMATING"), scale, labelColor, font);
        copiedLabel = UI_SafeTranslateString("EXE_COPIED");
        copiedText = va("(%s %s %s %s)", downloadedSize, UI_SafeTranslateString("EXE_OF"), totalSize, copiedLabel);
        Text_PaintCenter(centerPoint, yStart + 340.0f, copiedText, scale, labelColor, font);
    }
}

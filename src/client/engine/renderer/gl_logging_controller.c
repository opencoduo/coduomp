#include "gl_debug.h"
#include "gl_state.h"
#include "wgl_debug.h"

#include <time.h>

/* Original 0x00f92fc0. The value records the nonzero r_logFile countdown
 * present when logging is first enabled; subsequent calls only distinguish
 * zero from nonzero. */
static int32_t rendererGlLoggingState;

/* Source: CoDUOMP.exe 0x004df020..0x004e2dd0.
 * The controller installs exactly 508 same-signature logging wrappers when
 * logging transitions from off to on, and restores those 508 public entries
 * from their driver slots when it transitions back off. For every entry, the
 * original wrapper's indirect driver call uses the same driver slot restored
 * by this function. The original stack-cookie prologue/epilogue is compiler
 * machinery generated for the 1024-byte path buffer.
 *
 * While already active, each nonzero call decrements r_logFile and leaves the
 * wrappers installed until the cvar reaches zero. Stream creation preserves
 * the original time/localtime/asctime sequence and its extra blank line. */
void QGL_EnableLogging(int32_t enable)
{
    char logPath[MAX_OSPATH];

    if (rendererGlLoggingState != 0) {
        if (enable != 0) {
            ri.Cvar_Set("r_logFile", va("%d", r_logFile->integer - 1));
            enable = r_logFile->integer;
            if (enable != 0)
                return;
        }
    } else if (enable == 0) {
        return;
    }

    rendererGlLoggingState = enable;
    if (enable != 0) {
        if (rendererGlLogFile == NULL) {
            time_t timestamp;
            struct tm *localTime;
            cvar_t *basePath;

            (void)time(&timestamp);
            localTime = localtime(&timestamp);
            (void)asctime(localTime);

            basePath = ri.Cvar_Get("fs_basepath", "", CVAR_NONE);
            Com_sprintf(logPath, sizeof(logPath), "%s/gl.log", basePath->string);
            rendererGlLogFile = fopen(logPath, "wt");
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (rendererGlLogFile == NULL) {
                rendererGlLoggingState = 0;
                ri.Cvar_Set("r_logFile", "0");
                ri.Printf(R_PRINT_ALL, "QGL_EnableLogging: could not open %s\n", logPath);
                return;
            }
            fprintf(rendererGlLogFile, "%s\n", asctime(localTime));
            fflush(rendererGlLogFile);
        }

#define QGL_GL_ENTRY(type_, name_) \
    if (qgl##name_ != NULL) \
        qgl##name_ = GL_Log##name_;
#include "qgl_gl_entries.h"
#undef QGL_GL_ENTRY

#define QGL_WGL_ENTRY(type_, name_) \
    if (qwgl##name_ != NULL) \
        qwgl##name_ = WGL_Log##name_;
#include "qgl_wgl_entries.h"
#undef QGL_WGL_ENTRY
    } else {
        if (rendererGlLogFile != NULL) {
            fprintf(rendererGlLogFile, "*** CLOSING LOG ***\n");
            fclose(rendererGlLogFile);
            rendererGlLogFile = NULL;
        }

#define QGL_GL_ENTRY(type_, name_) qgl##name_ = rendererGl##name_##Driver;
#include "qgl_gl_entries.h"
#undef QGL_GL_ENTRY

#define QGL_WGL_ENTRY(type_, name_) qwgl##name_ = rendererWgl##name_##Driver;
#include "qgl_wgl_entries.h"
#undef QGL_WGL_ENTRY
    }
}

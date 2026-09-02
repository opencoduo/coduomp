#include "gl_debug.h"
#include "gl_state.h"
#include "wgl_debug.h"

#include "../platform/dynamic_library_boundary.h"
#if !defined(_WIN32)
#include "../platform/sdl_platform.h"
#endif

#include <string.h>

/* The original linker placed the 508 underlying driver pointers and the 508
 * public QGL/WGL dispatch pointers in one contiguous PE data range at
 * 0x04898c98..0x04899c74. Their source declarations remain individually
 * typed; the entry inventories keep definition, shutdown, and later loading
 * coverage synchronized without treating function pointers as object data. */
#define QGL_GL_ENTRY(type_, name_) \
    type_ rendererGl##name_##Driver; \
    type_ qgl##name_;
#include "qgl_gl_entries.h"
#undef QGL_GL_ENTRY

#define QGL_WGL_ENTRY(type_, name_) \
    type_ rendererWgl##name_##Driver; \
    type_ qwgl##name_;
#include "qgl_wgl_entries.h"
#undef QGL_WGL_ENTRY

void *rendererGlLibrary;

#if !defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: WGL's generic extension resolver is represented
 * by SDL_GL_GetProcAddress on native non-Windows contexts. The typed copy
 * keeps the function/object pointer conversion inside the platform boundary. */
static renderer_wgl_proc_t QGL_SDLGetProcAddress(const char *name)
{
    renderer_wgl_proc_t function = NULL;

    CoduoSDL_GetOpenGLSymbol(name, &function, sizeof(function));
    return function;
}
#endif

/* Source: CoDUOMP.exe 0x004f4e00..0x004f5316.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004f4e00_004f5317.mcode.
 * Role name: GLimp_Extensions calls this after accepting
 * GL_ARB_vertex_program. The original resolves these 62 entry points in the
 * exact inventory order and installs each result in both the driver and
 * public dispatch slots. */
void QGL_LoadARBVertexProgramFunctions(void)
{
#define QGL_ARB_VERTEX_PROGRAM_ENTRY(type_, name_) \
    rendererGl##name_##Driver = (type_)qwglGetProcAddress("gl" #name_); \
    qgl##name_ = rendererGl##name_##Driver;
#include "qgl_arb_vertex_program_entries.h"
#undef QGL_ARB_VERTEX_PROGRAM_ENTRY
}

/* Source: CoDUOMP.exe 0x004d7000..0x004d880e.
 * The Windows routine explicitly nulls each of the 1,016 typed dispatch
 * globals after releasing opengl32.dll. The inventories expand to those same
 * assignments while retaining native function-pointer types on every host. */
void QGL_Shutdown(void)
{
    ri.Printf(R_PRINT_ALL, "...shutting down QGL\n");

    if (rendererGlLibrary != NULL) {
        ri.Printf(R_PRINT_ALL, "...unloading OpenGL DLL\n");
        coduomp_library_close(rendererGlLibrary);
    }
    rendererGlLibrary = NULL;

#define QGL_GL_ENTRY(type_, name_) \
    rendererGl##name_##Driver = NULL; \
    qgl##name_ = NULL;
#include "qgl_gl_entries.h"
#undef QGL_GL_ENTRY

#define QGL_WGL_ENTRY(type_, name_) \
    rendererWgl##name_##Driver = NULL; \
    qwgl##name_ = NULL;
#include "qgl_wgl_entries.h"
#undef QGL_WGL_ENTRY
}

/* Source: CoDUOMP.exe 0x004d8810..0x004db12e.
 * QGL_Init resolves exactly 336 OpenGL 1.1 and 16 core WGL names. Each result
 * is stored in both the underlying driver slot and its public dispatch slot.
 * Before those loads, it nulls the 143 non-core GL pairs; the 13 non-core WGL
 * pairs are left untouched. The literal strstr("dllname", ".dll") is an
 * original source quirk visible in the executable and intentionally retained. */
qboolean QGL_Init(const char *dllName)
{
#if defined(_WIN32)
    char displayPath[MAX_OSPATH];
    char systemDirectory[MAX_OSPATH];
#endif
    int32_t logLevel;

#if defined(_WIN32)
    coduomp_system_library_directory(systemDirectory, sizeof(systemDirectory));
    ri.Printf(R_PRINT_ALL, "...initializing QGL\n");

    if (dllName[0] != '!' && strstr("dllname", ".dll") == NULL) {
        Com_sprintf(displayPath, sizeof(displayPath), "%s\\%s", systemDirectory, dllName);
    } else {
        strncpy(displayPath, dllName, sizeof(displayPath) - 1);
        displayPath[sizeof(displayPath) - 1] = '\0';
    }

    ri.Printf(R_PRINT_ALL, "...calling LoadLibrary( '%s.dll' ): ", displayPath);
    rendererGlLibrary = coduomp_library_open(dllName);
    if (rendererGlLibrary == NULL) {
        ri.Printf(R_PRINT_ALL, "failed\n");
        return qfalse;
    }
    ri.Printf(R_PRINT_ALL, "succeeded\n");
#else
    (void)dllName;
    ri.Printf(R_PRINT_ALL, "...initializing QGL through the native SDL OpenGL context\n");
#endif

    /* 0x004d8904..0x004d8fc4 nulls 286 slots: the driver/public pairs for
     * every non-core GL function, but none of the non-core WGL pairs. */
#if defined(_WIN32)
#define QGL_EXTENSION_GL_ENTRY(type_, name_) \
    rendererGl##name_##Driver = NULL; \
    qgl##name_ = NULL;
#include "qgl_extension_gl_entries.h"
#undef QGL_EXTENSION_GL_ENTRY
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native SDL reloads do not inherit the Win32
     * WGL lifecycle. Reset all typed slots before repopulating the native GL
     * core so a previous context cannot leave stale extension pointers. */
#define QGL_GL_ENTRY(type_, name_) \
    rendererGl##name_##Driver = NULL; \
    qgl##name_ = NULL;
#include "qgl_gl_entries.h"
#undef QGL_GL_ENTRY

#define QGL_WGL_ENTRY(type_, name_) \
    rendererWgl##name_##Driver = NULL; \
    qwgl##name_ = NULL;
#include "qgl_wgl_entries.h"
#undef QGL_WGL_ENTRY
#endif

#if defined(_WIN32)
#define QGL_CORE_GL_ENTRY(type_, name_) \
    do { \
        coduomp_library_symbol(rendererGlLibrary, "gl" #name_, &rendererGl##name_##Driver, sizeof(rendererGl##name_##Driver)); \
        qgl##name_ = rendererGl##name_##Driver; \
    } while (0);
#define QGL_CORE_WGL_ENTRY(type_, name_) \
    do { \
        coduomp_library_symbol(rendererGlLibrary, "wgl" #name_, &rendererWgl##name_##Driver, sizeof(rendererWgl##name_##Driver)); \
        qwgl##name_ = rendererWgl##name_##Driver; \
    } while (0);
#else
#define QGL_CORE_GL_ENTRY(type_, name_) \
    do { \
        CoduoSDL_GetOpenGLSymbol("gl" #name_, &rendererGl##name_##Driver, sizeof(rendererGl##name_##Driver)); \
        qgl##name_ = rendererGl##name_##Driver; \
    } while (0);
#define QGL_CORE_WGL_ENTRY(type_, name_) \
    do { \
        rendererWgl##name_##Driver = NULL; \
        qwgl##name_ = NULL; \
    } while (0);
#endif
#include "qgl_core_entries.h"
#undef QGL_CORE_WGL_ENTRY
#undef QGL_CORE_GL_ENTRY

#if !defined(_WIN32)
    rendererWglGetProcAddressDriver = QGL_SDLGetProcAddress;
    qwglGetProcAddress = QGL_SDLGetProcAddress;
    if (qglGetString == NULL || qglGetIntegerv == NULL) {
        ri.Printf(R_PRINT_ALL, "...native OpenGL context is missing core 1.1 entry points\n");
        QGL_Shutdown();
        return qfalse;
    }
#endif

    logLevel = r_logFile->integer;
    QGL_EnableErrorChecking(logLevel == 0 && r_debugGLErrors->integer != 0);
    QGL_EnableLogging(logLevel);
    return qtrue;
}

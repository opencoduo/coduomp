#include "gl_debug.h"
#include "gl_error_wrappers.h"
#include "wgl_debug.h"

/* Original 0x00f933c8. */
static qboolean rendererGlErrorCheckingEnabled;

/* Source: CoDUOMP.exe 0x004db3c0..0x004df01f.
 * The enable half tests each of the 508 current public GL/WGL entries before
 * replacing a non-null entry with its same-signature checked wrapper. The
 * disable half restores all 508 public entries from their underlying driver
 * slots. The typed inventories express the original 1,016 dispatch stores
 * without raw pointer addresses. The Windows compiler passes enable in EAX at
 * its internal call sites; maintained source uses the normal C ABI. */
void QGL_EnableErrorChecking(qboolean enable)
{
    if (rendererGlErrorCheckingEnabled == enable)
        return;

    rendererGlErrorCheckingEnabled = enable;
    if (enable != qfalse) {
#define QGL_GL_ENTRY(type_, name_) \
    if (qgl##name_ != NULL) \
        qgl##name_ = GL_Checked##name_;
#include "qgl_gl_entries.h"
#undef QGL_GL_ENTRY

#define QGL_WGL_ENTRY(type_, name_) \
    if (qwgl##name_ != NULL) \
        qwgl##name_ = WGL_Checked##name_;
#include "qgl_wgl_entries.h"
#undef QGL_WGL_ENTRY
    } else {
#define QGL_GL_ENTRY(type_, name_) qgl##name_ = rendererGl##name_##Driver;
#include "qgl_gl_entries.h"
#undef QGL_GL_ENTRY

#define QGL_WGL_ENTRY(type_, name_) qwgl##name_ = rendererWgl##name_##Driver;
#include "qgl_wgl_entries.h"
#undef QGL_WGL_ENTRY
    }
}

#include "filesystem.h"
#include "filesystem_path_security.h"
#include "filesystem_services.h"

/* Source: CoDUOMP.exe 0x0042df80..0x0042e044 and coduo_lnxded
 * 0x080625d1..0x080626b4. Both bodies consider active directory search paths,
 * probe the host file, and return "gamedir/qpath" through va storage. Native
 * case recovery and client host-path hardening remain target services. */
char *FS_ShortOSFilePath(const char *qpath)
{
    if (coduo_compat_path_is_safe_relative(qpath) == qfalse)
        return NULL;

    for (searchpath_t *search = fs_searchpaths;
         search != NULL;
         search = search->next) {
        if (FS_UseSearchPath(search) == qfalse || search->dir == NULL)
            continue;

        directory_t *const directory = search->dir;
        char osPath[MAX_OSPATH];
        FS_BuildOSPath(directory->path, directory->gamedir, qpath, osPath);
        FILE *const file = filesystem_compat_fopen_read(
            directory->path, osPath);
        if (file == NULL)
            continue;

        (void)fclose(file);
        return va("%s/%s", directory->gamedir, qpath);
    }

    return NULL;
}

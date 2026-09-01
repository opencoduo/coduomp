// Source: uo_cgame_mp_x86.dll 0x30027840..0x300279c7
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30027840_300279c7.mcode
// Writes a generated bank of numbered flame-sprite shader definitions.

#include <stdint.h>
#include <string.h>

#include "client/cgame/client_recovered.h"

void CG_WriteFlameShaderFile(const char *fileName, const char *shaderPrefix,
                             const char *spriteName, int32_t frameCount,
                             const char *sourceBlend, const char *destBlend,
                             const char *extraStageText, qboolean compressed,
                             qboolean noMipMaps)
{
    int32_t fileHandle;
    char shaderText[512];

    cgame_syscall(CG_FS_FOPEN_FILE, (intptr_t)fileName,
                  (intptr_t)&fileHandle, FS_WRITE);

    for (int32_t frame = 0; frame < frameCount; ++frame) {
        int32_t hundreds = frame / 100;
        int32_t tens = (frame % 100) / 10;
        int32_t ones = frame % 10;
        const char *mipDirective = noMipMaps
            ? (compressed ? "\n\tnomipmaps" : "\n\tnomipmap") : "";

        if (compressed) {
            Com_sprintf(shaderText, sizeof(shaderText),
                "%s%i\n{\n\tnofog%s\n\tallowCompress\n\tcull none\n\t{\n"
                "\t\tmapcomp gfx/sprites/%s_lg/spr%i%i%i.tga\n"
                "\t\tmapnocomp sprites/%s/spr%i%i%i.tga\n"
                "\t\tblendFunc %s %s\n%s\t}\n}\n",
                shaderPrefix, frame + 1, mipDirective,
                spriteName, hundreds, tens, ones,
                spriteName, hundreds, tens, ones,
                sourceBlend, destBlend, extraStageText);
        } else {
            Com_sprintf(shaderText, sizeof(shaderText),
                "%s%i\n{\n\tnofog%s\n\tallowCompress\n\tcull none\n\t{\n"
                "\t\tmap gfx/sprites/%s/spr%i%i%i.tga\n"
                "\t\tblendFunc %s %s\n%s\t}\n}\n",
                shaderPrefix, frame + 1, mipDirective,
                spriteName, hundreds, tens, ones,
                sourceBlend, destBlend, extraStageText);
        }

        cgame_syscall(CG_FS_WRITE, (intptr_t)shaderText,
                      (int32_t)strlen(shaderText), fileHandle);
    }
    cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
}

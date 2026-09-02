#include "client/engine/client/cgame.h"
#include "client/engine/client/console.h"
#include "sound/alias/sound_alias.h"
#include "client/engine/ui/ui_module_loader.h"

/* Source: CoDUOMP.exe 0x0041c230..0x0041c2fc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041c230_0041c2fd.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_ShutdownUI. The Windows optimizer inlines Com_UnloadSoundAliases and
 * VM_Free around the UI module's shutdown call. */
void CL_ShutdownUI(void)
{
    Com_UnloadSoundAliases(SND_ALIAS_BANK_COMMON);
    cls.keyCatchers &= ~KEYCATCH_UI;
    cls.uiStarted = qfalse;

    if (coduo_uiVm == NULL)
        return;

    (void)VM_Call(coduo_uiVm, UIVM_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    VM_Free(coduo_uiVm);
    coduo_uiVm = NULL;
}

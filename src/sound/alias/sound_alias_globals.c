#include "sound_alias_private.h"

/* Each engine binary links one private copy of the original sound-alias
 * runtime state. The Windows client and Linux dedicated server agree on the
 * three bank slots and 256 hash buckets; the fourth byte after the active
 * flags is alignment padding, not another bank. Windows addresses below are
 * recorded so moving the definitions does not discard their proven identity. */
uint8_t com_soundAliasBankActive[SND_ALIAS_BANK_COUNT]; /* 0x009678a4 */
snd_alias_t
    *com_soundAliasHash[SND_ALIAS_BANK_COUNT][SND_ALIAS_HASH_BUCKET_COUNT];
                                                       /* 0x009678a8 */
snd_alias_t *com_soundAliases[SND_ALIAS_BANK_COUNT];   /* 0x009684a8 */
int32_t com_soundAliasCount[SND_ALIAS_BANK_COUNT];     /* 0x009684b4 */
uint32_t com_soundAliasChecksum[SND_ALIAS_BANK_COUNT]; /* 0x00968504 */
char com_soundAliasSubtitleReference[
    SND_ALIAS_SUBTITLE_REFERENCE_CAPACITY];            /* 0x00968510 */
char *com_soundAliasCurrentFile;                       /* 0x009678a0 */
char com_soundAliasLocalizedSource[SND_ALIAS_SOURCE_NAME_CAPACITY];
                                                       /* 0x009684c4 */
snd_alias_parse_node_t *com_soundAliasBuildList;       /* 0x009684c0 */

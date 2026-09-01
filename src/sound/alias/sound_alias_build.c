#include <stdint.h>

#include "sound_alias_private.h"

#define SOUND_ALIAS_DISTANCE_MAX_DEFAULT_SCALE 5.0f

/* CoDUOMP.exe 0x00436fd0..0x00437123 and coduo_lnxded
 * 0x0806d4ae..0x0806d686; canonical name confirmed by the supporting Mac
 * engine symbol. */
void Com_FinishBuildingSoundAlias(snd_alias_parse_node_t *node)
{
    float value;

    if (node->pitchMax < node->pitchMin) {
        value = node->pitchMax;
        node->pitchMax = node->pitchMin;
        node->pitchMin = value;
    }

    if (node->pitchMin <= 0.0f) {
        Com_Error(ERR_DROP,
                  "\x15" "sound alias '%s' has pitch_min %g <= 0\n",
                  node->aliasName, node->pitchMin);
    }

    if (node->volumeMax < node->volumeMin) {
        value = node->volumeMax;
        node->volumeMax = node->volumeMin;
        node->volumeMin = value;
    }

    if (node->volumeMin < 0.0f) {
        Com_Error(ERR_DROP,
                  "\x15" "sound alias '%s' has vol_min < 0\n",
                  node->aliasName, node->volumeMin);
    }

    if (node->distanceMax == 0.0f) {
        node->distanceMax = node->distanceMin *
                            SOUND_ALIAS_DISTANCE_MAX_DEFAULT_SCALE;
    }

    if (node->distanceMax < node->distanceMin) {
        Com_Error(ERR_DROP,
                  "\x15" "sound alias '%s' has dist_min %g <= dist_max %g\n",
                  node->aliasName, node->distanceMin, node->distanceMax);
    }

    if (node->distanceMin <= 0.0f) {
        Com_Error(ERR_DROP,
                  "\x15" "sound alias '%s' has dist_min <= 0\n",
                  node->aliasName, node->distanceMin);
    }
}

/* CoDUOMP.exe 0x00437130..0x0043715f and coduo_lnxded
 * 0x0806d686..0x0806d6fc; canonical name confirmed by the supporting Mac
 * engine symbol. */
snd_alias_parse_node_t *
Com_AddBuildSoundAlias(const snd_alias_parse_node_t *node)
{
    snd_alias_parse_node_t *clone;

    clone = Hunk_AllocateTempMemoryInternal((size_t)sizeof(*clone));
    *clone = *node;
    clone->next = com_soundAliasBuildList;
    com_soundAliasBuildList = clone;

    return clone;
}

/* CoDUOMP.exe 0x00437160..0x0043721e and coduo_lnxded
 * 0x0806d6fc..0x0806d851; canonical name confirmed by the supporting Mac
 * engine symbol. */
void Com_AddSoundAlias(const snd_alias_parse_node_t *node,
                       snd_alias_t *alias,
                       const char *name,
                       const char *file,
                       const char *subtitle,
                       sndAliasBank_t bank)
{
    int bucket;

    alias->aliasName = name;
    alias->soundFile = file;
    alias->subtitle = subtitle;
    alias->soundFileInfo = NULL;
    alias->pickSequence = 0;
    alias->volumeMin = node->volumeMin;
    alias->volumeMax = node->volumeMax;
    alias->pitchMin = node->pitchMin;
    alias->pitchMax = node->pitchMax;
    alias->distanceMin = node->distanceMin;
    alias->distanceMax = node->distanceMax;
    alias->channel = node->channel;
    alias->type = node->type;
    alias->loop = node->loop;
    alias->isMaster = node->isMaster;
    alias->isSlave = node->isSlave;
    alias->slavePercentage = node->slavePercentage;
    alias->selectionWeight = node->selectionWeight;
    alias->lodMin = node->lodMin;
    alias->lodMax = node->lodMax;

    bucket = Com_HashAliasName(alias->aliasName);
    alias->hashNext = com_soundAliasHash[bank][bucket];
    com_soundAliasHash[bank][bucket] = alias;
}

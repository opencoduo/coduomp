#ifndef CODUOMP_SERVER_SERVER_H
#define CODUOMP_SERVER_SERVER_H

#include "../networking/net_channel.h"
#include "filesystem/filesystem.h"
#include "../q_shared.h"
#include "game_module_abi.h"
#include "qcommon/asset_type_names.h"
#include "qcommon/server_runtime_types.h"
#include "server/engine/server_commands.h"
#include "server/engine/server_authorize.h"
#include "server/engine/server_client_release.h"
#include "server/engine/server_client_gamestate.h"
#include "server/engine/server_client_maintenance.h"
#include "server/engine/server_client_message.h"
#include "server/engine/server_connect.h"
#include "server/engine/server_configstrings.h"
#include "server/engine/server_dobj.h"
#include "server/engine/server_download.h"
#include "server/engine/server_frame.h"
#include "server/engine/server_game_bridge.h"
#include "server/engine/server_game_data.h"
#include "server/engine/server_game_hunk.h"
#include "server/engine/server_game_lifecycle.h"
#include "server/engine/server_game_queries.h"
#include "server/engine/server_game_syscalls.h"
#include "server/engine/server_lifecycle.h"
#include "server/engine/server_master.h"
#include "server/engine/server_netchan.h"
#include "server/engine/server_operator_clients.h"
#include "server/engine/server_operator_maps.h"
#include "server/engine/server_operator_runtime.h"
#include "server/engine/server_packet.h"
#include "server/engine/server_punkbuster_queries.h"
#include "server/engine/server_snapshot_archive.h"
#include "server/engine/server_snapshot_send.h"
#include "server/engine/server_startup.h"
#include "server/engine/server_xmodel.h"
#include "collision/collision_server_entity.h"
#include "collision/collision_world_sector.h"
#include "qcommon/server_types.h"
#include "../vm_runtime_compat.h"

#include <stddef.h>
#include <stdint.h>

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(sharedEntity_t) == 4,
               "i386 shared-entity alignment changed");
_Static_assert(offsetof(sharedEntity_t, entityState) == 0x000,
               "original shared-entity state offset");
_Static_assert(sizeof(((sharedEntity_t *)0)->entityState) == 0x0f4,
               "original shared-entity state extent");
_Static_assert(offsetof(sharedEntity_t, linked) == 0x0f4,
               "original shared-entity linked offset");
_Static_assert(offsetof(sharedEntity_t, svFlags) == 0x0f8,
               "original shared-entity server-flags offset");
_Static_assert(offsetof(sharedEntity_t, singleClient) == 0x0fc,
               "original shared-entity single-client offset");
_Static_assert(offsetof(sharedEntity_t, soundTime) == 0x100,
               "original shared-entity sound-time offset");
_Static_assert(offsetof(sharedEntity_t, bmodel) == 0x104,
               "original shared-entity brush-model offset");
_Static_assert(offsetof(sharedEntity_t, mins) == 0x108,
               "original shared-entity minimum-bounds offset");
_Static_assert(sizeof(((sharedEntity_t *)0)->mins) == 0x0c,
               "original shared-entity minimum-bounds extent");
_Static_assert(offsetof(sharedEntity_t, maxs) == 0x114,
               "original shared-entity maximum-bounds offset");
_Static_assert(sizeof(((sharedEntity_t *)0)->maxs) == 0x0c,
               "original shared-entity maximum-bounds extent");
_Static_assert(offsetof(sharedEntity_t, contents) == 0x120,
               "original shared-entity contents offset");
_Static_assert(offsetof(sharedEntity_t, absMin) == 0x124,
               "original shared-entity absolute-minimum offset");
_Static_assert(sizeof(((sharedEntity_t *)0)->absMin) == 0x0c,
               "original shared-entity absolute-minimum extent");
_Static_assert(offsetof(sharedEntity_t, absMax) == 0x130,
               "original shared-entity absolute-maximum offset");
_Static_assert(sizeof(((sharedEntity_t *)0)->absMax) == 0x0c,
               "original shared-entity absolute-maximum extent");
_Static_assert(offsetof(sharedEntity_t, currentOrigin) == 0x13c,
               "original shared-entity current-origin offset");
_Static_assert(sizeof(((sharedEntity_t *)0)->currentOrigin) == 0x0c,
               "original shared-entity current-origin extent");
_Static_assert(offsetof(sharedEntity_t, currentAngles) == 0x148,
               "original shared-entity current-angle offset");
_Static_assert(sizeof(((sharedEntity_t *)0)->currentAngles) == 0x0c,
               "original shared-entity current-angle extent");
_Static_assert(offsetof(sharedEntity_t, ownerNum) == 0x154,
               "original shared-entity owner offset");
_Static_assert(sizeof(sharedEntity_t) == 0x158,
               "original host-visible shared-entity prefix size");
_Static_assert(_Alignof(serverReliableCommand_t) == 4,
               "i386 reliable-command alignment changed");
_Static_assert(offsetof(serverReliableCommand_t, commandText) == 0x000,
               "original reliable-command text offset");
_Static_assert(sizeof(((serverReliableCommand_t *)0)->commandText) == 0x400,
               "original reliable-command text extent");
_Static_assert(offsetof(serverReliableCommand_t, enqueueTime) == 0x400,
               "original reliable-command enqueue-time offset");
_Static_assert(offsetof(serverReliableCommand_t, reliable) == 0x404,
               "original reliable-command reliability offset");
_Static_assert(sizeof(serverReliableCommand_t) == 0x408,
               "original reliable-command size");
_Static_assert(_Alignof(serverAuthorizeGuidCacheEntry_t) == 4,
               "i386 authorization GUID-cache entry alignment changed");
_Static_assert(offsetof(serverAuthorizeGuidCacheEntry_t, numericGuid) == 0x00,
               "original authorization GUID-cache GUID offset");
_Static_assert(offsetof(serverAuthorizeGuidCacheEntry_t, cacheTime) == 0x04,
               "original authorization GUID-cache time offset");
_Static_assert(sizeof(serverAuthorizeGuidCacheEntry_t) == 0x08,
               "original authorization GUID-cache entry size");
_Static_assert(sizeof(archivedEntity_t) == 276,
               "original server-entity baseline size");
_Static_assert(_Alignof(serverCachedSnapshotClient_t) == 4,
               "i386 cached snapshot-client alignment changed");
_Static_assert(offsetof(serverCachedSnapshotClient_t, playerStateValid) == 0x0000,
               "original cached snapshot-client player-state-valid offset");
_Static_assert(offsetof(serverCachedSnapshotClient_t, snapshot) == 0x0004,
               "original cached snapshot-client metadata offset");
_Static_assert(sizeof(((serverCachedSnapshotClient_t *)0)->snapshot) == 0x5c,
               "original cached snapshot-client metadata extent");
_Static_assert(offsetof(serverCachedSnapshotClient_t, playerState) == 0x0060,
               "original cached snapshot-client player-state offset");
_Static_assert(
    sizeof(((serverCachedSnapshotClient_t *)0)->playerState) == 0x4504,
    "original cached snapshot-client player-state extent");
_Static_assert(sizeof(serverCachedSnapshotClient_t) == 0x4564,
               "original cached snapshot-client size");
_Static_assert(_Alignof(serverCachedSnapshotFrame_t) == 4,
               "i386 cached snapshot-frame alignment changed");
_Static_assert(offsetof(serverCachedSnapshotFrame_t, archivedFrameIndex) == 0x00,
               "original cached snapshot-frame archive-index offset");
_Static_assert(offsetof(serverCachedSnapshotFrame_t, messageTime) == 0x04,
               "original cached snapshot-frame message-time offset");
_Static_assert(offsetof(serverCachedSnapshotFrame_t, numEntities) == 0x08,
               "original cached snapshot-frame entity-count offset");
_Static_assert(offsetof(serverCachedSnapshotFrame_t, firstEntity) == 0x0c,
               "original cached snapshot-frame first-entity offset");
_Static_assert(offsetof(serverCachedSnapshotFrame_t, numClients) == 0x10,
               "original cached snapshot-frame client-count offset");
_Static_assert(offsetof(serverCachedSnapshotFrame_t, firstClient) == 0x14,
               "original cached snapshot-frame first-client offset");
_Static_assert(offsetof(serverCachedSnapshotFrame_t, decodedFromDelta) == 0x18,
               "original cached snapshot-frame delta-origin offset");
_Static_assert(sizeof(serverCachedSnapshotFrame_t) == 0x1c,
               "original cached snapshot-frame size");
_Static_assert(_Alignof(snapshotEntityNumbers_t) == 4,
               "i386 snapshot entity-number list alignment changed");
_Static_assert(offsetof(snapshotEntityNumbers_t, count) == 0x0000,
               "original snapshot entity-number count offset");
_Static_assert(offsetof(snapshotEntityNumbers_t, entityRefs) == 0x0004,
               "original snapshot entity-reference array offset");
_Static_assert(
    sizeof(((snapshotEntityNumbers_t *)0)->entityRefs) == 0x1000,
    "original snapshot entity-reference array extent");
_Static_assert(sizeof(snapshotEntityNumbers_t) == 0x1004,
               "original snapshot entity-number list size");
_Static_assert(_Alignof(archivedSnapshotFrameIndex_t) == 4,
               "i386 archived snapshot-frame index alignment changed");
_Static_assert(offsetof(archivedSnapshotFrameIndex_t, firstByte) == 0x00,
               "original archived snapshot-frame first-byte offset");
_Static_assert(offsetof(archivedSnapshotFrameIndex_t, byteCount) == 0x04,
               "original archived snapshot-frame byte-count offset");
_Static_assert(sizeof(archivedSnapshotFrameIndex_t) == 0x08,
               "original archived snapshot-frame index size");
_Static_assert(_Alignof(svEntity_t) == 4,
               "i386 server-entity alignment changed");
_Static_assert(offsetof(svEntity_t, worldSector) == 0x000,
               "original server-entity world-sector offset");
_Static_assert(offsetof(svEntity_t, nextInWorldSector) == 0x004,
               "original server-entity world-sector link offset");
_Static_assert(offsetof(svEntity_t, baseline) == 0x008,
               "original server-entity baseline offset");
_Static_assert(sizeof(((svEntity_t *)0)->baseline) == 0x114,
               "original server-entity baseline extent");
_Static_assert(offsetof(svEntity_t, numClusters) == 0x11c,
               "original server-entity cluster-count offset");
_Static_assert(offsetof(svEntity_t, clusterNums) == 0x120,
               "original server-entity cluster-array offset");
_Static_assert(sizeof(((svEntity_t *)0)->clusterNums) == 0x40,
               "original server-entity cluster-array extent");
_Static_assert(offsetof(svEntity_t, lastCluster) == 0x160,
               "original server-entity last-cluster offset");
_Static_assert(offsetof(svEntity_t, areaNum) == 0x164,
               "original server-entity primary-area offset");
_Static_assert(offsetof(svEntity_t, areaNum2) == 0x168,
               "original server-entity secondary-area offset");
_Static_assert(offsetof(svEntity_t, contentsMask) == 0x16c,
               "original server-entity contents offset");
_Static_assert(offsetof(svEntity_t, linkMins) == 0x170,
               "original server-entity link-minimum offset");
_Static_assert(sizeof(((svEntity_t *)0)->linkMins) == 0x08,
               "original server-entity link-minimum extent");
_Static_assert(offsetof(svEntity_t, linkMaxs) == 0x178,
               "original server-entity link-maximum offset");
_Static_assert(sizeof(((svEntity_t *)0)->linkMaxs) == 0x08,
               "original server-entity link-maximum extent");
_Static_assert(sizeof(svEntity_t) == 0x180,
               "original server-entity stride");
_Static_assert(_Alignof(worldSector_t) == 4,
               "i386 world-sector alignment changed");
_Static_assert(offsetof(worldSector_t, axis) == 0x00,
               "original world-sector axis offset");
_Static_assert(offsetof(worldSector_t, staticModelContentsMask) == 0x04,
               "original world-sector static-model mask offset");
_Static_assert(
    offsetof(worldSector_t, sightTraceStaticModelContentsMask) == 0x08,
    "original world-sector sight-trace static-model mask offset");
_Static_assert(offsetof(worldSector_t, entityContentsMask) == 0x0c,
               "original world-sector entity-mask offset");
_Static_assert(offsetof(worldSector_t, dist) == 0x10,
               "original world-sector split-distance offset");
_Static_assert(offsetof(worldSector_t, entityLinkHead) == 0x14,
               "original world-sector entity-head offset");
_Static_assert(offsetof(worldSector_t, staticModelLinkHead) == 0x18,
               "original world-sector static-model head offset");
_Static_assert(offsetof(worldSector_t, parent) == 0x1c,
               "original world-sector parent offset");
_Static_assert(offsetof(worldSector_t, children) == 0x20,
               "original world-sector children offset");
_Static_assert(sizeof(((worldSector_t *)0)->children) == 0x08,
               "original world-sector children extent");
_Static_assert(sizeof(worldSector_t) == 0x28,
               "original world-sector stride");
_Static_assert(_Alignof(worldSectorAreaLink_t) == 4,
               "i386 world-sector area-link alignment changed");
_Static_assert(offsetof(worldSectorAreaLink_t, model) == 0x00,
               "original world-sector area-link model offset");
_Static_assert(offsetof(worldSectorAreaLink_t, origin) == 0x04,
               "original world-sector area-link origin offset");
_Static_assert(sizeof(((worldSectorAreaLink_t *)0)->origin) == 0x0c,
               "original world-sector area-link origin extent");
_Static_assert(offsetof(worldSectorAreaLink_t, inverseAxis) == 0x10,
               "original world-sector area-link axis offset");
_Static_assert(
    sizeof(((worldSectorAreaLink_t *)0)->inverseAxis) == 0x24,
    "original world-sector area-link axis extent");
_Static_assert(
    offsetof(worldSectorAreaLink_t, nextInWorldSector) == 0x34,
    "original world-sector area-link next offset");
_Static_assert(offsetof(worldSectorAreaLink_t, linkMins) == 0x38,
               "original world-sector area-link mins offset");
_Static_assert(sizeof(((worldSectorAreaLink_t *)0)->linkMins) == 0x0c,
               "original world-sector area-link mins extent");
_Static_assert(offsetof(worldSectorAreaLink_t, linkMaxs) == 0x44,
               "original world-sector area-link maxs offset");
_Static_assert(sizeof(((worldSectorAreaLink_t *)0)->linkMaxs) == 0x0c,
               "original world-sector area-link maxs extent");
_Static_assert(
    offsetof(worldSectorAreaLink_t, sightTraceEligible) == 0x50,
    "original world-sector area-link sight-trace flag offset");
_Static_assert(sizeof(worldSectorAreaLink_t) == 0x54,
               "original world-sector area-link stride");
_Static_assert(_Alignof(clientSnapshot_t) == 4,
               "i386 client snapshot-frame alignment changed");
_Static_assert(offsetof(clientSnapshot_t, playerState) == 0x0000,
               "original client snapshot-frame player-state offset");
_Static_assert(
    sizeof(((clientSnapshot_t *)0)->playerState) == 0x4504,
    "original client snapshot-frame player-state extent");
_Static_assert(offsetof(clientSnapshot_t, numEntities) == 0x4504,
               "original client snapshot-frame entity-count offset");
_Static_assert(offsetof(clientSnapshot_t, numClients) == 0x4508,
               "original client snapshot-frame client-count offset");
_Static_assert(offsetof(clientSnapshot_t, firstEntity) == 0x450c,
               "original client snapshot-frame first-entity offset");
_Static_assert(offsetof(clientSnapshot_t, firstClient) == 0x4510,
               "original client snapshot-frame first-client offset");
_Static_assert(
    offsetof(clientSnapshot_t, messageSentTime) == 0x4514,
    "original client snapshot-frame sent-time offset");
_Static_assert(
    offsetof(clientSnapshot_t, messageAcknowledgedTime) == 0x4518,
    "original client snapshot-frame acknowledged-time offset");
_Static_assert(offsetof(clientSnapshot_t, messageSize) == 0x451c,
               "original client snapshot-frame message-size offset");
_Static_assert(sizeof(clientSnapshot_t) == 0x4520,
               "original client snapshot-frame size");
_Static_assert(_Alignof(serverClientDownload_t) == 4,
               "i386 client-download alignment changed");
_Static_assert(offsetof(serverClientDownload_t, fileName) == 0x000,
               "original client-download file-name offset");
_Static_assert(sizeof(((serverClientDownload_t *)0)->fileName) == 0x040,
               "original client-download file-name extent");
_Static_assert(offsetof(serverClientDownload_t, fileHandle) == 0x040,
               "original client-download file-handle offset");
_Static_assert(offsetof(serverClientDownload_t, fileSize) == 0x044,
               "original client-download file-size offset");
_Static_assert(offsetof(serverClientDownload_t, bytesRead) == 0x048,
               "original client-download bytes-read offset");
_Static_assert(
    offsetof(serverClientDownload_t, nextAcknowledgmentBlock) == 0x04c,
    "original client-download next-acknowledgment-block offset");
_Static_assert(offsetof(serverClientDownload_t, nextBufferedBlock) == 0x050,
               "original client-download next-buffered-block offset");
_Static_assert(offsetof(serverClientDownload_t, nextTransmitBlock) == 0x054,
               "original client-download next-transmit-block offset");
_Static_assert(offsetof(serverClientDownload_t, blockData) == 0x058,
               "original client-download block-window offset");
_Static_assert(sizeof(((serverClientDownload_t *)0)->blockData) == 0x020,
               "original client-download block-window extent");
_Static_assert(offsetof(serverClientDownload_t, blockByteCounts) == 0x078,
               "original client-download block-byte-count window offset");
_Static_assert(
    sizeof(((serverClientDownload_t *)0)->blockByteCounts) == 0x020,
    "original client-download block-byte-count window extent");
_Static_assert(offsetof(serverClientDownload_t, eofBlockQueued) == 0x098,
               "original client-download queued-EOF offset");
_Static_assert(
    offsetof(serverClientDownload_t, lastBlockActivityTime) == 0x09c,
    "original client-download block-activity-time offset");
_Static_assert(
    offsetof(serverClientDownload_t, redirectAllowedByClient) == 0x0a0,
    "original client-download client-redirect-permission offset");
_Static_assert(offsetof(serverClientDownload_t, redirectUrl) == 0x0a4,
               "original client-download redirect-URL offset");
_Static_assert(sizeof(((serverClientDownload_t *)0)->redirectUrl) == 0x100,
               "original client-download redirect-URL extent");
_Static_assert(offsetof(serverClientDownload_t, redirectActive) == 0x1a4,
               "original client-download redirect-active offset");
_Static_assert(
    offsetof(serverClientDownload_t, redirectAcknowledged) == 0x1a8,
    "original client-download redirect-acknowledged offset");
_Static_assert(offsetof(serverClientDownload_t, redirectFailed) == 0x1ac,
               "original client-download redirect-failed offset");
_Static_assert(sizeof(serverClientDownload_t) == 0x1b0,
               "original client-download size");
_Static_assert(_Alignof(client_t) == 4,
               "i386 server-client alignment changed");
_Static_assert(offsetof(client_t, state) == 0x00000,
               "original server-client state offset");
_Static_assert(offsetof(client_t, sendAsActive) == 0x00004,
               "original server-client active-send flag offset");
_Static_assert(offsetof(client_t, deferredDropReason) == 0x00008,
               "original deferred-drop-reason offset");
_Static_assert(offsetof(client_t, userinfo) == 0x0000c,
               "original server-client userinfo offset");
_Static_assert(sizeof(((client_t *)0)->userinfo) == 0x00400,
               "original server-client userinfo extent");
_Static_assert(offsetof(client_t, reliableCommands) == 0x0040c,
               "original reliable-command ring offset");
_Static_assert(sizeof(((client_t *)0)->reliableCommands) == 0x10200,
               "original reliable-command ring extent");
_Static_assert(offsetof(client_t, reliableSequence) == 0x1060c,
               "original reliable-sequence offset");
_Static_assert(offsetof(client_t, reliableAcknowledge) == 0x10610,
               "original reliable-acknowledgement offset");
_Static_assert(
    offsetof(client_t, reliableSent) == 0x10614,
    "original last-sent reliable-command offset");
_Static_assert(offsetof(client_t, messageAcknowledge) == 0x10618,
               "original message acknowledgement offset");
_Static_assert(offsetof(client_t, gamestateMessageNum) == 0x1061c,
               "original gamestate message-number offset");
_Static_assert(offsetof(client_t, challenge) == 0x10620,
               "original server-client challenge offset");
_Static_assert(offsetof(client_t, lastUsercmd) == 0x10624,
               "original server-client last-user-command offset");
_Static_assert(sizeof(((client_t *)0)->lastUsercmd) == 0x00018,
               "original server-client last-user-command extent");
_Static_assert(offsetof(client_t, lastClientCommand) == 0x1063c,
               "original last client-command offset");
_Static_assert(
    offsetof(client_t, lastClientCommandString) == 0x10640,
    "original last client-command string offset");
_Static_assert(
    sizeof(((client_t *)0)->lastClientCommandString) == 0x00400,
    "original last client-command string extent");
_Static_assert(offsetof(client_t, gentity) == 0x10a40,
               "original server-client entity offset");
_Static_assert(offsetof(client_t, name) == 0x10a44,
               "original server-client name offset");
_Static_assert(sizeof(((client_t *)0)->name) == 0x00020,
               "original server-client name extent");
_Static_assert(offsetof(client_t, download) == 0x10a64,
               "original server-client download offset");
_Static_assert(sizeof(((client_t *)0)->download) == 0x001b0,
               "original server-client download extent");
_Static_assert(offsetof(client_t, deltaMessage) == 0x10c14,
               "original delta-message offset");
_Static_assert(offsetof(client_t, nextReliableTime) == 0x10c18,
               "original next reliable-command time offset");
_Static_assert(offsetof(client_t, lastPacketTime) == 0x10c1c,
               "original last-packet-time offset");
_Static_assert(offsetof(client_t, lastConnectTime) == 0x10c20,
               "original last-connect-time offset");
_Static_assert(offsetof(client_t, nextSnapshotTime) == 0x10c24,
               "original next-snapshot-time offset");
_Static_assert(offsetof(client_t, rateDelayed) == 0x10c28,
               "original rate-delayed flag offset");
_Static_assert(offsetof(client_t, timeoutCount) == 0x10c2c,
               "original timeout-count offset");
_Static_assert(offsetof(client_t, snapshotFrames) == 0x10c30,
               "original snapshot-frame ring offset");
_Static_assert(sizeof(((client_t *)0)->snapshotFrames) == 0x8a400,
               "original snapshot-frame ring extent");
_Static_assert(offsetof(client_t, ping) == 0x9b030,
               "original server-client ping offset");
_Static_assert(offsetof(client_t, rate) == 0x9b034,
               "original server-client rate offset");
_Static_assert(offsetof(client_t, snapshotMsec) == 0x9b038,
               "original snapshot interval offset");
_Static_assert(offsetof(client_t, pureAuthState) == 0x9b03c,
               "original pure-authentication state offset");
_Static_assert(offsetof(client_t, netchan) == 0x9b040,
               "original server-client netchan offset");
_Static_assert(sizeof(((client_t *)0)->netchan) == 0x10040,
               "original server-client netchan extent");
_Static_assert(offsetof(client_t, guid) == 0xab080,
               "original server-client guid offset");
_Static_assert(offsetof(client_t, scriptId) == 0xab084,
               "original server-client script-object offset");
_Static_assert(sizeof(((client_t *)0)->scriptId) == 0x00002,
               "original server-client script-object extent");
_Static_assert(offsetof(client_t, paddingAfterScriptId) == 0xab086,
               "original padding after script-object offset");
_Static_assert(sizeof(((client_t *)0)->paddingAfterScriptId) == 0x00002,
               "original padding after script-object extent");
_Static_assert(offsetof(client_t, isTestClient) == 0xab088,
               "original server-client test-client flag offset");
_Static_assert(offsetof(client_t, serverId) == 0xab08c,
               "original server-client server-ID offset");
_Static_assert(offsetof(client_t, punkbusterGuid) == 0xab090,
               "original server-client PunkBuster GUID offset");
_Static_assert(sizeof(((client_t *)0)->punkbusterGuid) == 0x00021,
               "original server-client PunkBuster GUID extent");
_Static_assert(
    offsetof(client_t, paddingAfterPunkbusterGuid) == 0xab0b1,
    "original trailing server-client padding offset");
_Static_assert(
    sizeof(((client_t *)0)->paddingAfterPunkbusterGuid) == 0x00003,
    "original trailing server-client padding extent");
_Static_assert(sizeof(client_t) == 0xab0b4,
               "original server-client slot size");
_Static_assert(_Alignof(challenge_t) == 4,
               "i386 server challenge alignment changed");
_Static_assert(offsetof(challenge_t, address) == 0x00,
               "original server challenge address offset");
_Static_assert(sizeof(((challenge_t *)0)->address) == 0x14,
               "original server challenge address extent");
_Static_assert(offsetof(challenge_t, challengeNumber) == 0x14,
               "original server challenge value offset");
_Static_assert(offsetof(challenge_t, slotTimestamp) == 0x18,
               "original server challenge time offset");
_Static_assert(offsetof(challenge_t, pingStartTime) == 0x1c,
               "original server challenge ping-time offset");
_Static_assert(offsetof(challenge_t, authorizeStartTime) == 0x20,
               "original server challenge first-time offset");
_Static_assert(offsetof(challenge_t, firstPingMsec) == 0x24,
               "original server challenge first-ping offset");
_Static_assert(offsetof(challenge_t, connected) == 0x28,
               "original server challenge connected offset");
_Static_assert(offsetof(challenge_t, numericGuid) == 0x2c,
               "original server challenge GUID offset");
_Static_assert(offsetof(challenge_t, authGuidString) == 0x30,
               "original server challenge authorization-GUID offset");
_Static_assert(sizeof(((challenge_t *)0)->authGuidString) == 0x20,
               "original server challenge authorization-GUID extent");
_Static_assert(offsetof(challenge_t, unusedTail) == 0x50,
               "original server challenge unused-tail offset");
_Static_assert(sizeof(((challenge_t *)0)->unusedTail) == 0x04,
               "original server challenge unused-tail extent");
_Static_assert(sizeof(challenge_t) == 0x54,
               "original server challenge size");
_Static_assert(_Alignof(serverStatic_t) == 4,
               "i386 server-static alignment changed");
_Static_assert(offsetof(serverStatic_t, initialized) == 0x00000,
               "original svs.initialized offset");
_Static_assert(offsetof(serverStatic_t, time) == 0x00004,
               "original svs.time offset");
_Static_assert(offsetof(serverStatic_t, realTime) == 0x00008,
               "original svs.realTime offset");
_Static_assert(offsetof(serverStatic_t, snapFlagServerBit) == 0x0000c,
               "original svs.snapFlagServerBit offset");
_Static_assert(offsetof(serverStatic_t, clients) == 0x00010,
               "original svs.clients offset");
_Static_assert(offsetof(serverStatic_t, numEntityStateSnapshots) == 0x00014,
               "original entity-snapshot capacity offset");
_Static_assert(offsetof(serverStatic_t, numClientSnapshots) == 0x00018,
               "original client-snapshot capacity offset");
_Static_assert(offsetof(serverStatic_t, nextEntityStateSnapshot) == 0x0001c,
               "original next entity-snapshot offset");
_Static_assert(offsetof(serverStatic_t, nextClientSnapshot) == 0x00020,
               "original next client-snapshot offset");
_Static_assert(offsetof(serverStatic_t, entityStateSnapshots) == 0x00024,
               "original entity-snapshot ring pointer offset");
_Static_assert(offsetof(serverStatic_t, clientSnapshots) == 0x00028,
               "original client-snapshot ring pointer offset");
_Static_assert(offsetof(serverStatic_t, archiveEnabled) == 0x0002c,
               "original snapshot-archive enable offset");
_Static_assert(offsetof(serverStatic_t, nextArchivedSnapshotFrames) == 0x00030,
               "original next archived-frame offset");
_Static_assert(offsetof(serverStatic_t, archivedSnapshotFrames) == 0x00034,
               "original archived-frame ring pointer offset");
_Static_assert(offsetof(serverStatic_t, archivedSnapshotBuffer) == 0x00038,
               "original archived-buffer pointer offset");
_Static_assert(offsetof(serverStatic_t, nextArchivedSnapshotBuffer) == 0x0003c,
               "original next archived-buffer byte offset");
_Static_assert(offsetof(serverStatic_t, nextCachedSnapshotEntities) == 0x00040,
               "original next cached-entity offset");
_Static_assert(offsetof(serverStatic_t, nextCachedSnapshotClients) == 0x00044,
               "original next cached-client offset");
_Static_assert(offsetof(serverStatic_t, nextCachedSnapshotFrames) == 0x00048,
               "original next cached-frame offset");
_Static_assert(offsetof(serverStatic_t, cachedSnapshotEntities) == 0x0004c,
               "original cached-entity ring pointer offset");
_Static_assert(offsetof(serverStatic_t, cachedSnapshotClients) == 0x00050,
               "original cached-client ring pointer offset");
_Static_assert(offsetof(serverStatic_t, cachedSnapshotFrames) == 0x00054,
               "original cached-frame ring pointer offset");
_Static_assert(offsetof(serverStatic_t, nextHeartbeatTime) == 0x00058,
               "original next-heartbeat-time offset");
_Static_assert(offsetof(serverStatic_t, nextStatusResponseTime) == 0x0005c,
               "original next-status-response-time offset");
_Static_assert(offsetof(serverStatic_t, challenges) == 0x00060,
               "original server challenge-table offset");
_Static_assert(sizeof(((serverStatic_t *)0)->challenges) == 0x15000,
               "original server challenge-table extent");
_Static_assert(offsetof(serverStatic_t, redirectAddress) == 0x15060,
               "original remote-console redirect-address offset");
_Static_assert(sizeof(((serverStatic_t *)0)->redirectAddress) == 0x14,
               "original remote-console redirect-address extent");
_Static_assert(offsetof(serverStatic_t, authorizeServerAddress) == 0x15074,
               "original authorization-address offset");
_Static_assert(sizeof(((serverStatic_t *)0)->authorizeServerAddress) == 0x14,
               "original authorization-address extent");
_Static_assert(offsetof(serverStatic_t, netProfile) == 0x15088,
               "original server network-profile pointer offset");
_Static_assert(offsetof(serverStatic_t, authorizeGuidCache) == 0x1508c,
               "original authorization GUID-cache offset");
_Static_assert(sizeof(((serverStatic_t *)0)->authorizeGuidCache) == 0x80,
               "original authorization GUID-cache extent");
_Static_assert(sizeof(serverStatic_t) == 0x1510c,
               "original server-static size cleared by SV_Shutdown");
_Static_assert(_Alignof(serverHeader_t) == 4,
               "i386 server-header alignment changed");
_Static_assert(offsetof(serverHeader_t, state) == 0x00,
               "original sv.state offset");
_Static_assert(offsetof(serverHeader_t, restarting) == 0x04,
               "original sv.restarting offset");
_Static_assert(offsetof(serverHeader_t, serverId) == 0x08,
               "original sv.serverId offset");
_Static_assert(offsetof(serverHeader_t, gamestateChecksumFeed) == 0x0c,
               "original sv.gamestate-checksum-feed offset");
_Static_assert(sizeof(serverHeader_t) == 0x10,
               "original server-header prefix size");
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern cvar_t *sv_maxclients;       /* original 0x0491ccd0 */
extern cvar_t *g_gametype;          /* original 0x0491ccf4 */
extern cvar_t *sv_mapname;          /* original 0x0491ccf8 */
extern cvar_t *sv_mapRotationCurrent; /* original 0x0491cd0c */
extern cvar_t *scr_allow_tanks;     /* original 0x0491cd14 */
extern cvar_t *sv_mapRotation;      /* original 0x0491cd44 */
extern cvar_t *scr_allow_jeeps;     /* original 0x0491cd4c */
extern cvar_t *dedicated;           /* original 0x049290a4 */
extern cvar_t *sv_onlyVisibleClients; /* original 0x0491cd24 */
extern cvar_t *sv_kickBanTime;        /* original 0x0491cce4 */
extern cvar_t *sv_reconnectlimit;      /* original 0x0491cce0 */
extern cvar_t *sv_minPing;             /* original 0x0491cd18 */
extern cvar_t *sv_maxPing;             /* original 0x048a5670 */
extern cvar_t *sv_privateClients;      /* original 0x0491cccc */
extern cvar_t *sv_privatePassword;     /* original 0x0491cd20 */
extern cvar_t *sv_allowDownload;       /* original 0x0491ccec */
extern cvar_t *sv_maxRate;             /* original 0x0491cd00 */
extern cvar_t *sv_wwwDownload;         /* original 0x0491cd2c */
extern cvar_t *sv_wwwBaseURL;          /* original 0x0491cd38 */
extern cvar_t *sv_wwwDlDisconnected;   /* original 0x0491ccfc */
extern cvar_t *sv_pure;                /* original 0x04907bac */
extern cvar_t *sv_showCommands;        /* original 0x0491cd04 */
extern cvar_t *sv_floodProtect;        /* original 0x0491cce8 */
extern cvar_t *sv_hostname;            /* original 0x0491ccd4 */
extern cvar_t *sv_punkbuster;          /* original 0x0491cd48 */
extern cvar_t *sv_allowAnonymous;      /* original 0x0491ccdc */
extern cvar_t *sv_serverid;            /* original 0x0491cd40 */
extern cvar_t *rconPassword;           /* original 0x04907bb0 */
extern cvar_t *sv_fps;                 /* original 0x0491cd3c */
extern cvar_t *sv_timeout;             /* original 0x0491cd30 */
extern cvar_t *sv_zombietime;          /* original 0x0491cd1c */
extern cvar_t *sv_showloss;            /* original 0x0491ccd8 */
extern cvar_t *sv_padPackets;          /* original 0x0491cd34 */
extern cvar_t *sv_killserver;          /* original 0x0491ccf0 */
extern cvar_t *sv_packet_info;         /* original 0x0491cd08 */
extern cvar_t *sv_showAverageBPS;      /* original 0x0491cd28 */
extern char sv_gametypeNormalizeBuffer[MAX_QPATH];
extern serverStatic_t svs;          /* original 0x04907bc0 */
/* Declared by collision_server_entity.h; original 0x048a5680. */
extern vm_t *sv_gameVM; /* original 0x0389fdbc */
extern int32_t sv_serverId;         /* original 0x0389fdb8 */
extern int32_t sv_reconnectSequence; /* original 0x0389fdb4 */
extern qboolean sv_frameRunning;    /* original 0x0389fdc4 */
extern int32_t sv_timeResidual;     /* original 0x048a5690 */
extern char *sv_entityParsePoint;     /* original 0x04907a98 */
extern char *sv_configstrings[MAX_CONFIGSTRINGS];
/* World-sector storage is declared by collision_world_sector.h. */

void SV_DelayDropClient(client_t *client, const char *reason);
XAnimTree *Com_XAnimCreateTree(XAnim *tree);
XAnimTree *Com_XAnimCreateSmallTree(XAnim *tree);
void Com_XAnimFreeSmallTree(XAnimTree *tree);
void SV_XModelDebugBoxes(int32_t entityNum);
void SV_ProfDraw(const char *text, int32_t y);

#ifdef __cplusplus
}
#endif

#endif

#ifndef QCOMMON_PRECOMPILER_SERVICES_H
#define QCOMMON_PRECOMPILER_SERVICES_H

/* NOT_FROM_ORIGINAL_SOURCE: Windows-behavior parsing on the standalone host
 * retains the server's zone allocator and common log ownership.  These target
 * aliases affect host bookkeeping only; the selected parser layouts and
 * operation bodies remain the original Windows variants. */
#if defined(WINDOWS_BEHAVIOR)
#define GetMemory Com_ZoneDebugAlloc
#define GetClearedMemory Com_ZoneDebugAllocClear
#define FreeMemory Com_DebugFree
#define Log_Write Com_LogPrintf
#endif

#endif

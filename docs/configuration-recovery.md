# Configuration recovery

The improved client no longer recommends resetting settings because a previous
process did not quit cleanly. It checks config input directly and reports the
file, line, and reason when settings cannot finish loading. Stock is unchanged.

Each file is checked before any of its commands execute. Checks cover unfinished
quotes or block comments, embedded NUL bytes, unsupported text encodings,
invalid built-in argument counts, and execution limits. Unknown mod commands,
empty values, comments, legacy text bytes, UTF-8 with or without a BOM, and a
missing final newline are supported. Config backslashes are ordinary characters.
Nested files are checked when loaded; a failed include pauses saving even if
earlier commands already ran. Arbitrary script side effects are not rolled back.

When loading fails, the original config and its recovery history are protected
from automatic saves. The client offers a backup for this session, temporary
defaults, or exit. Neither session choice replaces the original. Console-only
clients report the failure and expose the same recovery commands.

Resetting to temporary defaults preserves the last complete, valid `cl_language`
assignment recoverable from the existing config, even when another command is
malformed. If none can be recovered, the current language preference is retained.
The retained preference is included when the resulting settings are exported.

| Console command | Effect |
| --- | --- |
| `config_status` | Show the active config path, saving state, and available recovery files. |
| `config_use_backup loaded` | Use the last successfully loaded snapshot for this session. Use `1`, `2`, or `3` for an older generation. |
| `config_defaults` | Use temporary defaults while keeping the original protected. |
| `config_retry` | Read and execute the primary file again after manual repair. Saving resumes only after loading finishes. |
| `config_restore loaded` | Preserve the current original under a unique name, replace it with the selected snapshot, and load it. Also accepts `1`, `2`, or `3`. |
| `writeconfig recovered.cfg` | Export current settings to a separate file without unlocking a protected primary. |

Recovery commands that change the session must be entered directly, rather
than invoked by a config script. A failed or uncertain save remains pending;
`config_status` gives the destination to inspect. Read-only files are respected.
If another writer changed the file, reload or repair it before saving again.

## Files beside the primary config

For `uoconfig_mp.cfg`, recovery files are:

- `uoconfig_mp.cfg.loaded.cfg`: settings captured after loading completes;
- `uoconfig_mp.cfg.backup1.cfg` through `.backup3.cfg`: previous saved versions,
  newest first; and
- `uoconfig_mp.cfg.original-*.cfg`: originals preserved by explicit restoration.

Managed snapshots store their version, byte count, and FNV-1a-64 checksum in
comment lines inside the snapshot file. The editable primary needs no checksum.
Restoration checks both snapshot completeness and config syntax. Preserved
originals are never automatically pruned. Interrupted transactions may leave
`.pending-*.cfg` or `.rollback-*.cfg` files; retain these when investigating a
failed save. A persistent `.lock` file coordinates writers; its presence alone
does not mean a process still holds the lock.

## Saving

Automatic saves coalesce changes for one second, with a five-second maximum
pending age once loading is complete. Disk work uses an owned snapshot on an
I/O worker. Explicit saves, normal quit, and profile departure request an
immediate save with a bounded wait. Failed saves retain their pending snapshot
and back off before retrying. A departed profile's retry keeps its own settings
and destination. Recovery sessions and incomplete loads remain protected.

Saves prepare a complete adjacent file, check writes and flushes, preserve the
previous bytes, then replace the primary. Linux synchronizes the file and parent
directory. macOS requests full device synchronization, falling back to `fsync`
only when full synchronization is unsupported. Windows flushes the prepared
file and uses `ReplaceFileW` for an existing destination. Child symbolic links
and Windows reparse points are unsupported config destinations.

These measures reduce corruption risk; storage hardware and filesystems still
limit power-loss guarantees. A file truncated between complete commands can
remain syntactically valid, and successful loading does not prove every setting
will work on the current hardware. Recovery snapshots provide additional options
without treating ordinary manual edits as corruption.

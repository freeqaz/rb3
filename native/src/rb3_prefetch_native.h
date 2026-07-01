// rb3_prefetch_native.h — preview-hover mogg prefetch + residency primitives.
//
// TASK T3 (incremental-load-perf). The cold-preview-hover freeze on web is
// SongPreview::PrepareSong's synchronous TheSynth->NewStream -> NewFile sync-XHR
// open of the FULL 32-37 MB preview mogg (the canvas is frozen for the whole
// network round-trip). This shim provides two primitives so the matched decomp
// can (1) warm the mogg async during the ~1 s preview debounce window and
// (2) defer the blocking NewStream until the bytes are MEMFS-resident:
//
//   RB3PreviewPrefetchEnabled() — read RB3_PREVIEW_PREFETCH_OFF once (default ON).
//   RB3PrefetchMogg(rel)        — kick an async fetch of server-relative mogg
//                                 path `rel` (e.g. "songs/x/x.mogg"); web only,
//                                 in-flight deduped; native = no-op.
//   RB3MoggResident(rel)        — true once the bytes are present at /data/<rel>;
//                                 native = always true (host file is resident).
//   RB3PrefetchCancel()         — drop the in-flight tracking on selection
//                                 change / preview stop (the fetch itself can't
//                                 be aborted, but we stop waiting on it and let
//                                 a new hover dedupe-or-refetch cleanly).
//
// All entry points are safe to call on native (no __EMSCRIPTEN__): the prefetch
// is a no-op and residency is always true, so the SongPreview state machine
// traverses the new fetch-pending state in exactly one frame and behaves
// identically to today.
#pragma once

#ifdef HX_NATIVE

// Returns false when RB3_PREVIEW_PREFETCH_OFF is set (restores today's behavior:
// no prefetch, synchronous stream construction). Read once via static init.
bool RB3PreviewPrefetchEnabled();

// Kick an async prefetch of the server-relative mogg path (no leading slash,
// e.g. "songs/foo/foo.mogg"). In-flight dedupe: a second call for the same path
// while the first is still pending is a no-op. No-op on native and when the
// prefetch flag is off.
void RB3PrefetchMogg(const char *serverRelMoggPath);

// True once the mogg at the given server-relative path is resident (web: present
// in MEMFS at /data/<rel>; native: always true). Empty/null path => true so the
// gate never wedges on a bad input.
bool RB3MoggResident(const char *serverRelMoggPath);

// Forget the currently-tracked in-flight prefetch (selection change / stop).
void RB3PrefetchCancel();

#endif // HX_NATIVE

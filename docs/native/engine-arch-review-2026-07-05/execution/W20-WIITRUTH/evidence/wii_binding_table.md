# Wii binding table — Lane W (W20-WIITRUTH)

**Platform = wii. Schema per A10.** Rows are per-(state, member, mesh, boneSlot) in the JSON;
this md aggregates per mesh for readability (full per-slot rows + A7 basis matrices in the
JSON). owningDirClass: OWN_MEMBER = `char/main/skeleton_unshared.milo` (per-member); 
SHARED_ROOT = `char/main/skeleton.milo` (the magnet); UNRESOLVED = ObjPtr deref failed
(NEVER a dir class — fail-red). Instance ids are opaque guest addresses.

> **HEADLINE: every resolved hand/outfit bone binds OWN_MEMBER. SHARED_ROOT = 0 in every
> state. The shared `skeleton.milo` magnet EXISTS in the heap (1 instance) but receives
> ZERO bone bindings.**

### State: gameplay/song-load (richest — includes hands_naked) (`gameplay_songload`)

- BandCharacter instances: **4**
- Shared `skeleton.milo` heap dir instances: **1** ['0x92b52d54'] (the magnet — EXISTS but see bindings)
- `skeleton_unshared.milo` heap dir instances: **5** ['0x92b5312c', '0x92c3e900', '0x92c5b180', '0x92c77748', '0x92c93d10']
- Distinct dir instances bones actually bind to: **3**
- Class counts: **{'OWN_MEMBER': 60}**
- gender_gap: True

| mesh | nBones | OWN | SHARED | UNRESOLVED | owningDirClass | dir instance(s) |
|---|---|---|---|---|---|---|
| hands_naked.mesh | 6 | 6 | 0 | 0 | OWN_MEMBER | ['0x92c5b180'] |
| leatherplaid_resource.2.mesh | 21 | 21 | 0 | 0 | OWN_MEMBER | ['0x92c5b180'] |
| leatherplaid_resource.mesh | 9 | 9 | 0 | 0 | OWN_MEMBER | ['0x92c5b180'] |
| rolledjeans_resource.2.mesh | 6 | 6 | 0 | 0 | OWN_MEMBER | ['0x92c77748'] |
| strapjacket_resource_WII.1.mesh | 18 | 18 | 0 | 0 | OWN_MEMBER | ['0x92c3e900'] |

### State: main_hub (clean, stable) (`main_hub`)

- BandCharacter instances: **4**
- Shared `skeleton.milo` heap dir instances: **1** ['0x92b52d54'] (the magnet — EXISTS but see bindings)
- `skeleton_unshared.milo` heap dir instances: **5** ['0x92b5312c', '0x92c3e678', '0x92c5aef8', '0x92c774c0', '0x92c93a88']
- Distinct dir instances bones actually bind to: **2**
- Class counts: **{'OWN_MEMBER': 32}**
- gender_gap: True

| mesh | nBones | OWN | SHARED | UNRESOLVED | owningDirClass | dir instance(s) |
|---|---|---|---|---|---|---|
| flarejeans_resource.3.mesh | 8 | 6 | 0 | 2 | OWN_MEMBER | ['0x92c93a88'] |
| flarejeans_resource.mesh | 5 | 5 | 0 | 0 | OWN_MEMBER | ['0x92c93a88'] |
| jacketvestnoshirt_resource.mesh | 21 | 21 | 0 | 0 | OWN_MEMBER | ['0x92c774c0'] |

### State: loading/preview (`loading_preview`)

- BandCharacter instances: **4**
- Shared `skeleton.milo` heap dir instances: **1** ['0x92b52d54'] (the magnet — EXISTS but see bindings)
- `skeleton_unshared.milo` heap dir instances: **5** ['0x92b5312c', '0x92c3e678', '0x92c5aef8', '0x92c774c0', '0x92c93a88']
- Distinct dir instances bones actually bind to: **2**
- Class counts: **{'OWN_MEMBER': 32}**
- gender_gap: True

| mesh | nBones | OWN | SHARED | UNRESOLVED | owningDirClass | dir instance(s) |
|---|---|---|---|---|---|---|
| flarejeans_resource.3.mesh | 8 | 6 | 0 | 2 | OWN_MEMBER | ['0x92c93a88'] |
| flarejeans_resource.mesh | 5 | 5 | 0 | 0 | OWN_MEMBER | ['0x92c93a88'] |
| jacketvestnoshirt_resource.mesh | 21 | 21 | 0 | 0 | OWN_MEMBER | ['0x92c774c0'] |

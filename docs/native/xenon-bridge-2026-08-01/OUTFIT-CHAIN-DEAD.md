# The entire outfit texture chain is dead in native — `RndDir::SyncObjects()` never runs

**Measured 2026-08-04, rb3 native (`rb3-native`, gameplay, guitarist closeup).**
This supersedes the framing of task #30 ("re-key `RB3Quad.cpp`'s compose onto the
real blend sequence"). That task is **moot as written**: the code it proposed to
re-key never executes, and every A/B that could have "verified" the re-key would
have been vacuous.

## What was measured

Five `RB3_COMPOSE_PROBE=1` probe points, each `#ifdef HX_NATIVE`-guarded and
each verified to leave the Wii match at 100% (`SyncObjects__6RndDirFv`,
`SyncDrawables__6RndDirFv`, `DrawPreClear__12OutfitConfigFv`,
`UpdatePreClearState__12OutfitConfigFv` — all COMPLETE 100.0% fuzzy + raw after
the edits).

Harness: `scripts/native/band-closeup-capture.py --member guitar`, verdict PASS,
**15/15 shots pinned**, 5 shots × 3 frames, 0 band drops. Engine log 250–360 KB,
5.4k–7.4k lines per run. Every run below had its probe string confirmed present
in the binary (`strings rb3-native | grep -c`) and the binary's mtime confirmed
*earlier* than the log's — so each zero is a real zero and not a stale binary or
a build that silently didn't happen.

| Probe point | Calls observed |
|---|---|
| `RndDir::SyncObjects()` | **0** |
| `RndDir::SyncDrawables()` | **0** |
| `OutfitConfig::UpdatePreClearState()` | **0** |
| `OutfitConfig::DrawPreClear()` | **0** |
| `OutfitConfig::MatSwap::Compose()` (entry) | **0** |
| `BandRnd::DrawRect` with `gRB3OutfitComposeActive` | **0** |

Also 0 with `RB3_SKIN_RTT=1` (which re-enables the skin composite), so the
result is not an artefact of the default skin bypass.

## The chain, and where it breaks

```
DirLoader::…  src/system/obj/DirLoader.cpp:781   mDir->SyncObjects()      <-- suspected break
  -> RndDir::SyncObjects()            Dir.cpp:51    (0 calls)
     -> [if (!IsSubDir())] SyncDrawables()  Dir.cpp:119   (0 calls)
        -> ObjDirItr<RndDrawable> walk -> it->UpdatePreClearState()
           -> OutfitConfig::UpdatePreClearState()  (0 calls)
              -> TheRnd->PreClearDrawAddOrRemove(this, true, false)
                 -> [Rnd's pre-clear draw list]
                    -> Rnd::DrawPreClear()   Rnd.cpp:751  it->DrawPreClear()
                       -> OutfitConfig::DrawPreClear()    (0 calls)
                          -> MatSwap::SwapResource()      (0 calls)
                          -> MatSwap::Compose()           (0 calls)
```

`Rnd::DrawPreClear()` **is** dispatched every frame (engine
`Rnd_Wgpu_RB3.cpp:2055`, default-ON, opt-out `RB3_NO_PRECLEAR=1`). It is not the
problem. The list it iterates is simply always empty, because nothing is ever
registered into it — `OutfitConfig::UpdatePreClearState()` is the only thing that
registers an `OutfitConfig`, and it is reached **only** from
`RndDir::SyncDrawables()`'s `ObjDirItr<RndDrawable>` walk.

The probe in `SyncDrawables` sits **above** the `if (!IsSubDir())` gate and still
never fired, so this is not the subdir early-out. Same for `SyncObjects`. The
break is at or above `DirLoader.cpp:781`, whose call is itself gated on
`IsLoaded() && mDir`.

## Why this matters more than the compose re-key

Everything downstream that has been worked on is a **workaround stacked on a dead
chain**:

- `MatSwap::SwapResource()` is what replaces the placeholder `*_resource.mat`
  binding. It never runs — which is why `dummy_torso/legs/feet.tex` (8×8 DXT
  magenta) survive to the screen. **This is the pink band.** The earlier X22
  result (pink 2670px → 80px, a 97% reduction) came from dispatching
  `SwapResource` *manually* out of the `RB3_X22_SWAP_RESOURCE` harness, i.e. from
  bypassing the dead chain rather than repairing it.
- The `RB3_SKIN_RTT` bypass in `SetSkinTextures` exists because the composite
  "bakes FLAT GREY skin". Flat grey is the last-layer-wins signature of the
  `SetColorModFlags`-instead-of-`SetBlend` decomp bug that was fixed on
  2026-08-04 — but with the chain dead, that fix cannot express itself either.
- The `gRB3OutfitComposeActive` collapse in engine `RB3Quad.cpp` is **doubly**
  dead. See below.

## The compose collapse in `RB3Quad.cpp` is unreachable by construction

Independent of the dead chain, the collapse could no longer fire even if compose
ran. It discriminates on `mat->mColorModFlags` (branches for `==3` and `==2`),
but:

- `OutfitConfig::sMat = Hmx::Object::New<RndMat>()` — freshly constructed, never
  `Copy`'d, never loaded from a file.
- `RndMat`'s constructor sets `mColorModFlags(kColorModNone)` (`Mat.cpp:45`).
- `SetColorModFlags` has **exactly one caller in the entire codebase**
  (`Crowd.cpp:304`), and it never touches `sMat`.

So `colorMod == 0` on all four compose passes, always. Only the `== 0` branch can
run; it does bookkeeping and falls through to `blend = mat->GetBlend()`.

And that fall-through is already **correct**: `WgpuBlend` is value-identical to
`RndMat::Blend`, and `PipelineManager::MapBlend` already maps `SrcAlpha(3)` to
`SrcAlpha/OneMinusSrcAlpha` and `Multiply(6)` to `Dst/Zero`. The now-fixed
`Compose` issues exactly `kBlendSrc(1) → kBlendSrcAlpha(3) → kBlendSrcAlpha(3)
→ kBlendMultiply(6)`, which those blend states realize as
`diff * lerp(color1, color2, interp.a)` — the authored recolor — with **no
bespoke code at all**.

**Conclusion: the compose collapse was compensating for the decomp bug, and the
decomp fix retires it.** Deleting it is correct, but it should be deleted as part
of the change that makes compose *run*, so the deletion can actually be A/B'd.
Deleting it now would be unverifiable. It is left in place deliberately.

Bonus: removing the cluster also retires the `W31-EXIT-TRAP` hazard —
`mComposeDiffView` transitively held the last strong ref to the Dawn
Device/Adapter/Instance (`Rnd_Wgpu_RB3.cpp:1099-1115`).

## Next (in order)

1. **Find why `DirLoader.cpp:781` doesn't reach `SyncObjects()`** — probe
   `IsLoaded()` / `mDir` at that site, and check whether native loads dirs
   through a different path entirely. This is the single blocking defect.
2. Once dirs sync: confirm `SwapResource` + `Compose` run unaided, and re-measure
   the pink pixel count **without** the `RB3_X22_SWAP_RESOURCE` harness.
3. Then delete the `RB3Quad.cpp` compose collapse + the `gRB3OutfitComposeActive`
   `ComposeScope` in `OutfitConfig.cpp`, with a real A/B.
4. Then re-evaluate whether `RB3_SKIN_RTT`'s default should flip back.

## Instrument

The probes are retained (env-gated `RB3_COMPOSE_PROBE=1`, `HX_NATIVE`-only,
match-verified) because step 1 needs exactly them:

```bash
RB3_COMPOSE_PROBE=1 python3 scripts/native/band-closeup-capture.py \
    --member guitar --frames 2 --out /tmp/bc-X --tag X
grep -E '\[(syncobjects|syncdraw|syncdraw-walk|preclear-register|preclear-probe|compose-entry)\]' \
    /tmp/rb3-bandcloseup-X-*.log
```

⚠ Before reading any zero from these as a result, confirm the probe string is in
the binary and the binary predates the log. Every zero above was checked that
way; the first attempt at this investigation reported a green build that had in
fact failed (`BUILD_RC` had captured `tail`'s exit code through a pipe).

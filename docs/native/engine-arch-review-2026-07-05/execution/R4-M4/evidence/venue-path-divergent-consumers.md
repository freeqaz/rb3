# Venue-path (eng_hot) divergent gRand consumers — seam-incompleteness attribution

Source: 3 fixed-song-time eng_hot guard-OFF boots under the seam
(`/tmp/r4m4-white/wr_val_OFF_0{1,2,3}.engine.log`), post-anchor window [anchor,anchor+300],
offsets resolved via addr2line against `native/build-agent-R4-M4/rb3-native`.

`postAnchorDelta = [16, 0, 16]` (ledger stream 2/3, NOT 10/10).

## The gRand-stream residual (±16) is fully explained by NON-isolated consumers

R4's M1 isolated the consumers divergent on the DEFAULT menu→gameplay path
(`part`/`camshot`/`chareyes`/`randgroupseq`). The eng_hot ENGAGED-VENUE path activates
ADDITIONAL per-frame gRand consumers that R4 never isolated — so they reach gRand and
the stream axis cannot reach 10/10 here:

| consumer (reaches gRand, un-isolated) | per-boot draws | spread |
|---|---|---|
| `CharClipDriver::CharClipDriver` (CharClipDriver.cpp:62) | [8, 0, 8] | 8 |
| `WorldCrowd::OnIterateFrac` (Crowd.cpp:1234, Fisher-Yates) | [7, 0, 7] | 7 |
| `CharInterest::ComputeScore` (CharInterest.cpp:172) | [4, 5, 4] | 1 |
| `LightPresetManager::PickRandomPreset` (LightPresetManager.cpp:286) | [1, 0, 1] | 1 |

8 + 7 + 1 = 16 = the observed `postAnchorDelta` spread. `WorldCrowd::OnIterateFrac` is
the Crowd shuffle PLAN-R4 §2.2 flagged as "location right, liveness unproven" — here
PROVEN live on the venue path.

## The render-outcome residual is a SEPARATE, larger axis (private stream)

`RndParticleSys::InitParticle` draws [2505, 1755, 2509] (~30% swing) — but it is
R4-isolated onto its private `part` stream, so those 754 draws do NOT reach gRand
(they don't move `postAnchorDelta`). The particle-emission COUNT/timing still varies
boot-to-boot = the `callerOrder` axis R4 reports 1/10 and leaves non-gating, owned by
W0.3d part-b (loader/worker completion-order determinism). BOOTRNG named particle/pyro
+ swept-light phase as the WHITE driver -> stream-matching does not collapse WHITE.

## Consequence
Two named seam-incompleteness axes on the eng_hot WHITE-measurement path, BOTH requiring
edits outside Lane W's writable set (src/engine):
  1. extend R4 consumer isolation to the venue path
     (CharClipDriver / WorldCrowd::OnIterateFrac / CharInterest / LightPresetManager);
  2. close W0.3d part-b for the particle-emission-count axis.
Until (1), the ledger stream axis cannot reach 10/10 on the eng_hot path -> the WHITE
re-grade boots are VOID per A2/F6 (harness refuses a verdict).

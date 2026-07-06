# Reviewer checklist — RB3_HUB_MENU_QUAD_HIDE flip decision

Walk each item against `captures/` before flipping the default. This mirrors the format of the
Wave-5 W2.1-flip checklist (the one whose item 3 caught the wash false-positive) — don't sign off
on a montage that fails its own listed check.

- [ ] **1. Quad is genuinely absent, not just dimmed/moved, under flag-ON.**
      Look at `quad_roi_off_vs_on.png`: OFF shows a solid opaque light-grey bar; ON shows the
      venue backdrop (MUSIC sign, brick storefront) drawing through where the bar was. Confirm
      it's gone, not translucent-behind-something.
- [ ] **2. A/A stability — both arms are boot-noise-tolerant.**
      `montage_2x2_off_vs_on.png`: OFF row (2 boots) both show the quad in the same position;
      ON row (2 boots) both show it absent. If either row disagrees with itself, STOP — that's a
      new finding (flake), not a clean flip, and should be treated the way Wave 5's asymmetric
      bloom blow-out was (held, not flipped, until characterized).
- [ ] **3. PLAY NOW label renders identically regardless of flag state (the blocker check named
      in the stage brief).** `label_roi_all_4.png`, read top to bottom: PLAY NOW / QUICKPLAY /
      START A ROAD CHALLENGE text is present, legible, and pixel-similar in all four boots. If any
      of the four is missing text, faded, or repositioned relative to the others, this **is** a
      blocker — do not flip, file it back to the Lane C owner instead of packaging as a flip
      candidate. (As captured: all four pass — see README finding 2.)
- [ ] **4. Retail has no analog of the quad.** `retail_vs_flagon.png` /
      `retail_vs_flagoff.png`: confirm by eye that the retail `main_hub` shot has nothing occupying
      the same screen region as the native capsule. (As captured: confirmed — retail's PLAY NOW
      menu stack is a text list with no separate mid-screen bar element.)
- [ ] **5. Flag-OFF is provably the unflipped default's current behavior, not a stale build.**
      Check the README's "Build under test" section states the engine/rb3 commits captured
      against, and that neither is a wave-stale checkout.
- [ ] **6. No collision with the W4.2 UI-text-floor decision.** Per the Wave-7 kickoff amendment
      A7, `RB3_HUB_MENU_QUAD_HIDE` was pinned OFF during W4.2's own captures so the two decisions
      don't confound each other. Confirm this package's captures are similarly single-variable
      (only `RB3_HUB_MENU_QUAD_HIDE` toggled; W4.2's `RB3_UI_TEXT_FLOOR_RELAXED` left at its
      current default in every capture here — see README "Build under test").
- [ ] **7. Fence respected.** `git show --stat` on this stage's commit(s) touches only
      `docs/native/engine-arch-review-2026-07-05/execution/W4.1/hub-quad-flip/` — no source, no
      engine, no classification.json, no default flipped.

**If all seven pass:** the actual gate is `src/band3/meta_band/MainHubPanel.cpp:130`,
`static bool sHideHubMenuQuad = !!getenv("RB3_HUB_MENU_QUAD_HIDE");` — a plain opt-in read, not a
`kFooDefaultOn` bool constant. The flip is therefore: invert the sense to an opt-*out* read (e.g.
`sHideHubMenuQuad = !getenv("RB3_HUB_MENU_QUAD_HIDE_OFF")`), matching the opt-out-first pattern
this campaign already used for `RB3_PLACEMENT_CONTRACT` (W2.1-flip) and `RB3_BLACK_HEAD_FIX_OFF`
(W2.7) — then flip `NativeCompatFlags.classification.json`'s `"default": "off"` → `"on"` for
`RB3_HUB_MENU_QUAD_HIDE` (or rename the row to the new opt-out key, matching whichever naming
convention those two prior flips used) + one classification regen. This package does not perform
that flip.

# Coordinator note for the Wave-13 C2b+C4 lane (written mid-wave, 2026-07-07)

The Wave-12 C34 side agent completed AFTER your dispatch (rb3 `c7f101f7`,
`execution/W4.3-C34/STATUS.md`, checkpoint /tmp/wave12-checkpoints/C34.json). Its C4 findings
bear directly on your work — read its STATUS before diagnosing C4:

- **C4 is (at least partly) NOT an anchor/offset bug:** the overlapping label is `message.lbl`
  (found via a UILabel::DrawShowing probe — the UIList-level probe watches the wrong draw path).
  Its world xfm sits ~6u from the sibling `expand_message_area.ihp` vs ~20.8u for a confirmed
  stacked pair — but the decisive retail comparison shows retail renders the message body at a
  **visibly smaller font, wrapped to 2 lines**; native renders it label-sized, no wrap. Root
  cause = a text SCALE/WRAP-WIDTH application gap in rb3-side code (Text.cpp / UILabel.cpp /
  AppLabel.cpp / BandLabel.cpp), not the engine mesh path. The ~6u spacing may be CORRECT for a
  small wrapped font.
- So for C4: pick up where C34 stopped — find the property that sets the message body's font
  scale + wrap width (authored in the milo / set by MainHubPanel code) and why it isn't applied
  natively. The "same family as C2b" hypothesis is likely FALSE for C4; test C2b on its own merits.
- C3 (flipped hold-labels) is NOT_A_BUG (faithful flip-card animation) — no work needed there.

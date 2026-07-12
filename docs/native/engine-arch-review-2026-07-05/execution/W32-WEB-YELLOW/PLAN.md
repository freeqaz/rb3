# W32-WEB-YELLOW — PLAN

Lane A (primary). Base SHA rb3 `30546499`, engine pin `24c4f95`.

## Charter
Target: the floating yellow-green quad over the lead character's torso on WEB
`main_hub_screen` (overshell `joined_default`), STATIC across focus moves;
native clean. STEP-0 (blocking): NAME the quad + the web-vs-native divergence
point BEFORE any fix.

## Constraints binding this lane
- **A1:** no drawlog/uidump on web (`rb3_http_server.cpp` native-only). Web
  naming = Playwright console + `window.rb3*` exports + screenshots + (if
  needed) own env-gated default-OFF draw probe in owned web glue. Native CONTROL
  = real `/api/uidump` + `/api/drawlog`.
- **A2:** `native/src/rb3_render_hook.cpp` is Lane C EXCLUSIVE-WRITE. If the
  divergence point lands in that TU (mechanism (i)), checkpoint
  COORDINATOR-ACK-NEEDED and STOP — no concurrent writes.
- **A10 (anti-gaming):** fix must act at the NAMED divergence point, not a
  position/size/screen-region suppression. After-fix evidence must replay
  `options`→`joined_default` with focus travel both directions.
- Owned: `scripts/web/`, `native/web/`, web-specific `native/src/` glue.
  SHARED (needs native A/B + Wii .o neutrality): `OvershellDir.cpp`.

## Steps
1. STEP-0a: reproduce the quad on the web debug build at `joined_default`;
   screenshot; confirm it is static across focus moves (both directions). DONE.
2. STEP-0b: name the quad's mesh (draw evidence). DONE (see STATUS).
3. STEP-0c: name the web-vs-native divergence point (file:line). DONE.
4. Adjudicate against A2: divergence == mechanism (i) render-hook family →
   checkpoint COORDINATOR-ACK-NEEDED, STOP before fix. Present fix proposal.

## Acceptance legs (kickoff + A10)
(1) quad named + draw evidence; (2) web-only mechanism named w/ file:line;
(3) after-fix web pair (quad gone, focus tracks both dirs); (4) native control
unchanged; (5) no B8 regression. Legs (1)(2) are STEP-0 (this lane's mandate);
(3)(4)(5) are post-fix, gated on coordinator arbitration per A2.

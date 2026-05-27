# rb3/native — Rock Band 3 (Wii) native port

This directory holds the **native, clang-LP64 port glue** for Rock Band 3 (Wii
build `SZBE69_B8`). It consumes the shared `../../milo-native-engine` runtime
via CMake `add_subdirectory` with a soft SHA pin, and links it against RB3's
matched-fork engine core plus a small set of per-decomp shims.

The native build is a *separate* target from the decomp's MWCC ninja build. The
matched fork in `../src/system/**` stays authoritative for asm-match
verification; the native build compiles those same `.cpp` files under clang with
`HX_NATIVE` gating (which is never defined for the MWCC build, so the ninja
build is unaffected). See [`../docs/native/NATIVE_PORT_ROADMAP.md`](../docs/native/NATIVE_PORT_ROADMAP.md)
for the full plan and the three-layer source model.

## `rb3-dta` — Phase 0.3 milestone (a)

`rb3-dta` is a **headless DTA parser**. It proves that RB3's matched-fork DTA
parse path (`obj/DataNode`, `obj/DataArray`, the flex-generated `DataFlex.c`
lexer, plus the `utl`/`os`/`math` core it pulls in) runs correctly under clang
LP64 against **real RB3 game data** — 138 on-disc songs parsed from
arkhelper-extracted assets:

```sh
# from rb3/native, after extracting assets via scripts/milo/extract_ark.sh:
build/rb3-dta orig-assets/extracted/songs/songs.dta 138
# -> parses all 138 top-level nodes (20th Century Boy, Bohemian Rhapsody,
#    Crazy Train, ... + pro-keys tutorial entries), prints id/name/artist, exit 0
```

### Milestone-(a) engine link shape

Milestone (a) links the engine **without injected decomp context**. The
`rb3/native/CMakeLists.txt` intentionally does *not* set
`MILO_ENGINE_DECOMP_INCLUDE_DIRS`, so `MILO_ENGINE_HAVE_CONTEXT` stays **OFF**
and the engine builds only its **placeholder `libmilo-engine.a`** — no gfx, no
Dawn, no audio. (Those are deferred to Phase 2.) The DTA-parse executable is
assembled from:

- **Matched-fork engine core** (globbed from `../src/system/`): `obj/`, `utl/`,
  `os/`, `math/`. Platform-specific TUs (`*_Xbox/_Wii/_Gekko.cpp`, GX/Wgpu/D3D),
  the `DataFlex_target.cpp` tester lexer, and off-DTA-path Wii backends
  (NAND/HTTP/CDReader/AsyncFile/HolmesClient) are filtered out.
- **The flex DTA lexer** `../src/system/obj/DataFlex.c` (compiled as C).
- **Native shims** in `src/`: `rvl_shims.cpp` (Wii SDK → POSIX),
  `native_link_glue.cpp` (`ObjRefConcrete<T>::CopyRef` instantiations),
  `native_file.cpp` (stdio-backed `File` for the read path), and
  `dta_link_stubs.s` (weak no-op stubs for off-path rendering/audio/Wii-manager
  symbols so global ctors don't crash on null at static-init time).
- **MWCC-compat clang flags**: `-fms-compatibility` / `-fms-extensions` /
  `-fdelayed-template-parsing` plus `-include src/mwcc_compat.h`, compiled with
  `HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1`.
- Wii SDK shim headers live in `src/revolution/`.

## Build

```sh
# from rb3/native/
cmake -B build
cmake --build build        # produces build/rb3-dta
```

Requires Clang (the build warns otherwise). `Threads` is required; `ZLIB` is
linked if found. The engine pin lives in `CMakeLists.txt` as `MILO_ENGINE_PIN`
(currently `9a58e86aa41e22a9fb90b4af39675eefd09a717e`); a mismatch with the
engine checkout's `git HEAD` warns but does not fail.

## Next: Phase 0.3 milestone (b) / Phase 1

The next session flips `rb3-native` to the **full-engine link**: inject decomp
context (`MILO_ENGINE_HAVE_CONTEXT=ON`, full MWCC context injection) so the
whole engine compiles against RB3's Milo headers, link it, and run to a
controlled exit. From there, Phase 1 lands the `.milo` scene-tree dump — load an
RB3 `.milo` file and print its object tree to stdout. The HX_NATIVE branches
that path needs are largely ports of what DC3 already did. See
[`../docs/native/NATIVE_PORT_ROADMAP.md`](../docs/native/NATIVE_PORT_ROADMAP.md)
(§0.3 and Phase 1) and the disposition catalog in
[`../docs/native/NATIVE_PORT_INVENTORY.md`](../docs/native/NATIVE_PORT_INVENTORY.md).

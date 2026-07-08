# Wave-20 Lane N — evidence regeneration commands

All probes are HX_NATIVE + env-gated default-OFF (`RB3_LOADBIND_PROBE`). Wii build
byte-identical. Engine pin `6e6387c` untouched. Binary: `native/build-native/rb3-native`.

## Build (probes compile in but inert unless RB3_LOADBIND_PROBE=1)
```
flock /tmp/rb3-native-build.lock -c 'cmake --build native/build-native --target rb3-native'
```

## Regression gate (must PASS with probes OFF)
```
python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order
# -> PASS (canonical-order): live capture matches golden (792 draws)
```

## Control arm — shim ON (shipped default)
```
RB3_LOADBIND_PROBE=1 RB3_FIXED_CLOCK=1 \
  python3 scripts/native/keyboard-to-gameplay.py --port <FREE> --diff hard \
  --out /tmp/wave20-N/control-shots
# engine log -> /tmp/rb3-kbd2game-<PORT>.log ; grep LOADBIND -> loadbind_control_shimON.log
```

## Reconciliation arm — shim OFF (retail kMerge)
```
RB3_LOADBIND_PROBE=1 RB3_LOADBIND_NOSHIM=1 RB3_FIXED_CLOCK=1 \
  python3 scripts/native/keyboard-to-gameplay.py --port <FREE> --diff hard \
  --out /tmp/wave20-N/noshim-shots
# grep LOADBIND -> loadbind_noshim_shimOFF.log
```

## A10 table + branch counters
```
python3 build_a10_table.py loadbind_control_shimON.log "native:main_hub+gameplay:shimON"
python3 build_a10_table.py loadbind_noshim_shimOFF.log "native:main_hub+gameplay:shimOFF"
```

## Probe env vars (all default-OFF)
- `RB3_LOADBIND_PROBE=1` — enables ALL Lane N logging (LOADBIND_*).
- `RB3_LOADBIND_NOSHIM=1` — disables ONLY the FilterSubdir kMerge→kReplace override
  (:4280) for the shim-reconciliation arm. Independent of the probe flag.

## Probe sites (all committed, default-OFF)
- src/system/bandobj/BandCharacter.cpp: branch counters + LOADBIND_BR2 (Filter :4182/4188/4202),
  LOADBIND_SUBDIR + NOSHIM (FilterSubdir), LOADBIND_INSTALL + LOADBIND_COUNTERS
  (OnInstallFilter), NativeLoadBindDumpMeshes → LOADBIND_MESH/LOADBIND_SLOT (entry of both
  rebind fns), NativeLoadBindDumpCounters (global).
- src/system/rndobj/Mesh.cpp: LOADBIND_RESOLVE (parse-time bs>>mBones, hand meshes).

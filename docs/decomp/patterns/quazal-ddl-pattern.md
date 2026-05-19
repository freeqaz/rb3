# Quazal `_DDL_X` Extract/Add Pattern

Discovered Wave 69E-70A. Took ~10 MISSING units to Matching in two waves.

## The Insight

`Extract` and `Add` methods on `_DDL_X` classes (in `network/Services/` and `network/Extensions/`) are **`static` member functions**, not non-static. The mangled symbol is identical (`Extract__7_DDL_XFP7MessageP15X`), but using non-static adds an implicit `this` in r3, shifting every arg register by one and producing a 70-80% match that looks "almost right."

```cpp
// WRONG: non-static (looks correct, gets ~79%)
class _DDL_Foo {
    void Extract(Message *m, Foo *out);
};

// RIGHT: static (gets 100%)
class _DDL_Foo {
    static void Extract(Message *m, Foo *out);
};
```

## The Body Pattern

For raw POD fields:
```cpp
msg->Extract((unsigned char *)&data->field, sizeof(field), 1);
```

For `String` / `StationURL` / `Buffer` fields, use the typed helpers in `src/network/ObjDup/DOCoreTypes.h`:
```cpp
_Type_string::Extract(msg, &data->stringField);
_Type_stationurl::Extract(msg, &data->urlField);
```

For `DateTime`, use a raw u64 tmp + a separate `DateTime tmp` and assign through union:
```cpp
unsigned long long raw;
DateTime tmp;
msg->Extract(&raw, 8, 1);
tmp.m_ui64Value = raw;
data->m_dt = tmp;
```

For `DOHandle`, copy `mValue` byte-by-byte from a separate local (slot allocation matters):
```cpp
unsigned int v = h.mValue;
msg->Append((const unsigned char*)&v, 4, 1);
```

## Add() Mirror

`Add` reverses the direction (calls `msg->Append(...)`). Same static signature, same per-type helpers.

## When to Use

Find MISSING units via:
```bash
python3 -c "
import json
o = json.load(open('config/SZBE69_B8/objects.json'))
for unit, info in o.get('units', {}).items():
    if 'network' not in unit: continue
    s = info if isinstance(info, str) else info.get('status','')
    if s == 'MISSING' and 'DDL' in unit: print(unit)
"
```

Then check asm size — anything under ~1000 lines is wave-sized.

## Skip These

- `PrivateDataDDL`: RTTI-only, table won't emit without forcing vtable instantiation.
- `RVConnectionDataDDL`, `GatheringStatsDDL`, `GatheringURLsDDL`: each requires `qList<T>::clear()` + `push_back` inlining match (intrusive-list alloc patterns) — non-trivial.

## Reference Files

Look at any of these for canonical examples:
- `src/network/Services/InvitationDDL.{cpp,h}` — simplest (2 ints + String)
- `src/network/Extensions/VoiceChannelMemberDDL.{cpp,h}` — DOHandle pair (Add + Extract)
- `src/network/Services/FriendDataDDL.{cpp,h}` — mixed POD + String

# GemPlayer::Pass Analysis

**Symbol:** `Pass__9GemPlayerFifib`
**Unit:** `game/GemPlayer`
**Size:** 1564 bytes
**Match:** 3.9%
**Date:** 2026-03-31

---

## Function Signature

```cpp
virtual void Pass(int track, float ms, int gem_id, bool cur_track);
```

Mangled name: `Pass__9GemPlayerFifib`

Parameters:
- `track` (r4): Track number (int)
- `ms` (f1): Timestamp in milliseconds (float)
- `gem_id` (r5): Gem index in the track
- `cur_track` (r6): Boolean flag indicating if this is the current track

---

## What This Function Does

`Pass()` is the gem miss/pass handler in the GemPlayer class. It's called when a gem is missed during normal gameplay. The function:

1. **Checks if gem has been processed** – Uses the `0x40` bit in `GemStatus` to detect if gem already dealt with (e.g., via rollback)
2. **Handles first gem after rollback** – Performs special initialization
3. **Marks section end** – Sets `unk3e1` flag if this is the last gem in the section
4. **Processes solo gems** – Checks if the gem is part of a solo phrase via `mBehavior->mHasSolos`
5. **Ignores gems in specific contexts** – If gem is in an ignorable fill or has certain flags set
6. **Marks gems as passed** – Sets the `0x4` processed flag
7. **Updates crowd meter and statistics** – Via `UpdateSectionStats()` and `PassGem()`
8. **Tracks penalization** – May penalize the player depending on gem type (phrase notes, drum cymbals)
9. **Sends game messages** – Posts "pass" message to other systems via Delta Tracking if enabled

---

## m2c Pseudocode

The m2c decompilation shows the high-level flow:

```c
void Pass__9GemPlayerFifib(GemPlayer *this, s32 arg0, f32 arg1, s32 arg2, s32 arg3) {
    // arg0 = track
    // arg1 = ms
    // arg2 = gem_id
    // arg3 = cur_track (bool)

    // Check if gem already has 0x40 bit set (indicates processed/rolled-back)
    if (arg2 == -1) {
        var_r0 = 0;
    } else {
        var_r0 = *(*this->unk2D0 + arg2) & 0x40;
    }

    if (var_r0 == 0) {  // Only process if NOT already marked 0x40
        HandleFirstGemAfterRollback__9GemPlayerFi(this, arg2);

        if (arg2 == (s32)(this->unk2D0->unk4 - 1)) {
            this->unk3E1 = 1;  // Mark end of section
        }

        if (arg3 != 0) {  // If cur_track is true
            // Get gem data
            temp_r26 = GetGem__6SongDBCFii(TheSongDB, this->unk248, arg2)->unk4;

            // Check if player is in solo/cymbal section
            if ((s32)this->unk268 != 0) {
                temp_r3 = this->unk374;
                // Check cymbal/solo detection
                if ((*temp_r3)->unk3C(temp_r3, temp_r26, 1) == 0 &&
                    this->unk4->unk2C(this) == 0) {
                    this->unk4->unk128(this);
                    DealWithCodaGem__4BandFP6Playeribb(this->unk1DC,
                                                        (Player *)this, arg2, 0, 0);
                }
            }

            // Check if gem should be ignored
            if (IgnoreGemsAt__9GemPlayerFi(this,
                    GetGem__6SongDBCFii(TheSongDB, this->unk248, arg2)->unk4) != 0) {
                IgnoreGem__9GemPlayerFi(this, arg2);
                return;
            }

            // Check if in ignorable fill
            if (InIgnorableFill__9GemPlayerFi(this, temp_r26) != 0) {
                Pass__8GemTrackFi(this->unk368, arg2);
                IgnoreGem__9GemPlayerFi(this, arg2);
                return;
            }

            // Check if gem has 0x8 flag (ignored?)
            if (var_r0_2 == 0) {
                temp_r3_2 = this->unk368;
                if (temp_r3_2 != NULL) {
                    Pass__8GemTrackFi(temp_r3_2, arg2);
                }
                // Increment pass counter
                temp_r5 = this->unk2D0;
                temp_r5->unkC = (s32)(temp_r5->unkC + 1);

                UpdateSectionStats__9GemPlayerFv(this);

                // Record gem pass in statistics
                temp_r3_3 = GetGem__6SongDBCFii(TheSongDB, this->unk248, arg2);
                PassGem__13StatCollectorFfRC7GameGemi(&this->unk3CC, arg1,
                                                       temp_r3_3, arg2);

                // Check if gem is cymbal (bit 6 set)
                if (((u8)temp_r3_3->unk10 >> 6U) & 1) {
                    this->unk188 += 1;
                }
            }

            // Check 0xE bits for phrase notes
            temp_r4 = this->unk2D0;
            if (arg2 == -1) {
                var_r0_3 = 1;
            } else {
                var_r0_3 = *(temp_r4->unk0 + arg2) & 0xE;
            }

            if (var_r0_3 == 0) {  // Not a phrase note
                if (arg2 != -1) {
                    temp_r3_4 = temp_r4->unk0;
                    *(temp_r3_4 + arg2) = *(temp_r3_4 + arg2) | 4;
                }

                if (ShouldPenalizeGem__9GemPlayerCFi(this, arg2) != 0) {
                    Penalize__9GemPlayerFfif(this, arg1, arg2, @F_00000000);
                } else {
                    this->unk4->unk58(this);
                    HandleCommonPhraseNote__9GemPlayerFii(this, 0, arg2);
                }

                this->unk4->unk60(this, arg2);
                this->unk3BC += 1;
            } else {  // IS a phrase note
                var_r25 = 1;
                if (arg2 != 0) {
                    temp_r26_2 = GetPhraseID__6SongDBCFii(TheSongDB,
                                                           this->unk248, arg2 - 1);
                    if (GetPhraseID__6SongDBCFii(TheSongDB, this->unk248, arg2)
                        == temp_r26_2) {
                        var_r25 = 0;
                    }
                }

                var_r24 = 1;
                if (arg2 != (s32)(GetNumPhraseIDs__6SongDBCFi(TheSongDB,
                                                               this->unk248) - 1)) {
                    temp_r27 = GetPhraseID__6SongDBCFii(TheSongDB,
                                                        this->unk248, arg2 + 1);
                    if (GetPhraseID__6SongDBCFii(TheSongDB, this->unk248, arg2)
                        == temp_r27) {
                        var_r24 = 0;
                    }
                }

                // Check 0x2 flag and if phrase start/end
                if (arg2 == -1) {
                    var_r0_4 = 0;
                } else {
                    var_r0_4 = *(this->unk2D0->unk0 + arg2) & 2;
                }

                if (((var_r0_4 != 0) && (var_r25 != 0)) ||
                    ((var_r24 != 0) && ((s32)TheGame->unk27 != 0))) {
                    HandleCommonPhraseNote__9GemPlayerFii(this, 0, arg2);
                }

                if (arg2 != -1) {
                    temp_r3_5 = this->unk2D0->unk0;
                    *(temp_r3_5 + arg2) = *(temp_r3_5 + arg2) | 4;
                }
            }

            var_r5 = 0;
            this->unk18 += 1;  // Increment some counter

            // Check if delta tracking is enabled
            if ((s32)TheGame->unk24 != 0) {
                if (arg2 == -1) {
                    var_r0_5 = 0;
                } else {
                    var_r0_5 = *(this->unk2D0->unk0 + arg2) & 8;
                }
                if (var_r0_5 != 0) {
                    var_r5 = 1;
                }
            }

            // Send pass message via delta tracking
            if (var_r5 == 0) {
                if ((s8)@GUARD@Pass__9GemPlayerFifib@passMsg == 0) {
                    // Initialize static message with pass symbol
                    sp20 = NULL;
                    sp24 = 6;
                    __ct__6SymbolFPCc(&sp8, "delta tracking not enabled\n\0...");
                    @LOCAL@Pass__9GemPlayerFifib@passMsg.unk0 = &__vt__7Message;
                    // Create DataArray for message parameters
                    var_r3 = _PoolAlloc__Fii8PoolType(0x10, 0x10, (PoolType)1);
                    if (var_r3 != NULL) {
                        var_r3 = __ct__9DataArrayFi(var_r3, 3);
                    }
                    @LOCAL@Pass__9GemPlayerFifib@passMsg.unk4 = var_r3;
                    // ... setup message array elements ...
                    @GUARD@Pass__9GemPlayerFifib@passMsg = 1;
                }

                sp18 = arg2;
                sp1C = 6;
                __as__8DataNodeFRC8DataNode(Node__9DataArrayFi(
                    @LOCAL@Pass__9GemPlayerFifib@passMsg.unk4, 2),
                    (DataNode *)&sp18);

                // Send the message
                this->unk210->unk20(&this->unk20C,
                    @LOCAL@Pass__9GemPlayerFifib@passMsg.unk4, 0);
            }

            // Call remote handler
            this->unk4->unk2D4(this, arg0, arg2, arg3, arg1);
        }
    }
}
```

---

## Related Functions

### Ignore (Comparison)

Located at `GemPlayer.cpp:383-398`. Pattern shows how gems are handled when ignored:

```cpp
void GemPlayer::Ignore(int i1, float ms, int gem_id, const UserGuid &u) {
    if (!mGemStatus->Get0x40(gem_id)) {
        HandleFirstGemAfterRollback(gem_id);
        int size = mGemStatus->GetSize();
        if (gem_id == size - 1) {
            unk3e1 = 1;
        }
        IgnoreGem(gem_id);
        if (mBehavior->mHasSolos) {
            HandleSoloGem(gem_id, false, ms, false);
        }
        if (mTrack) {
            mTrack->Ignore(gem_id);
        }
    }
}
```

**Similarities:**
- Both check the `0x40` bit before processing
- Both call `HandleFirstGemAfterRollback()`
- Both check if gem is the last in section (`unk3e1`)
- `Pass()` is much more complex with additional scoring/tracking logic

### Hit

Located at `GemPlayer.cpp:339-344`. Currently a stub:

```cpp
void GemPlayer::Hit(int, float, int, unsigned int gem_hit_slots, GemHitFlags) {
    MILO_ASSERT(gem_hit_slots != 0, 0x23F);
    MILO_WARN("hit");
    MILO_WARN("drum_trainer_unmute");
    MILO_WARN("send_hit");
}
```

---

## Key Observations from m2c

1. **Gem Status Bits** (tracked via `GemStatus::mGems[]`):
   - `0x1`: Hit flag
   - `0x2`: Missed flag
   - `0x4`: Processed flag (set when gem is dealt with)
   - `0x8`: Ignored flag
   - `0x10`: Hopoed flag
   - `0x40`: Unknown (checked before processing) — likely indicates already handled
   - `0x80`: Solo flag

2. **Member Variables Used**:
   - `unk2D0` (offset 0x2D0): Pointer to `GemStatus` (gem flag array)
   - `unk248` (offset 0x248): Track number for `TheSongDB` queries
   - `unk268` (offset 0x268): Solo/cymbal detection flag
   - `unk3E1` (offset 0x3E1): Section end marker
   - `unk3BC` (offset 0x3BC): Counter (possibly miss count)
   - `unk188` (offset 0x188): Cymbal counter
   - `unk18` (offset 0x18): Another counter
   - `unk210` (offset 0x210): Message handler
   - `unk20C` (offset 0x20C): Message context
   - `unk374` (offset 0x374): Cymbal/solo detector
   - `unk368` (offset 0x368): `GemTrack` pointer
   - `unk3CC` (offset 0x3CC): `StatCollector` for statistics

3. **External Dependencies**:
   - `GetGem()`, `GetGemID()`, `GetPhraseID()`, `GetNumPhraseIDs()` from `SongDB`
   - `PassGem()` from `StatCollector`
   - `Pass()` from `GemTrack`
   - `HandleFirstGemAfterRollback()`, `IgnoreGem()`, etc. (static member functions)
   - `DealWithCodaGem()` from `Band` class
   - `TheGame` and `TheSongDB` globals

4. **Delta Tracking Message**:
   - Creates a static Message object with "delta tracking not enabled" string
   - Message contains array with symbol "pass" and gem_id
   - Only sent if `TheGame->unk24` is 0 (delta tracking disabled) and gem doesn't have `0x8` flag

---

## Proposed C++ Implementation

Based on the m2c output and Ignore pattern:

```cpp
void GemPlayer::Pass(int track, float ms, int gem_id, bool cur_track) {
    // Check if gem already processed (0x40 bit)
    if (!mGemStatus->Get0x40(gem_id)) {
        HandleFirstGemAfterRollback(gem_id);

        // Mark section end if last gem
        int size = mGemStatus->GetSize();
        if (gem_id == size - 1) {
            unk3e1 = 1;
        }

        if (cur_track) {
            // Get gem from song database
            const GameGem *gem = TheSongDB->GetGem(mTrackNum, gem_id);
            s32 gem_slot = gem->unk4;

            // Handle solo/cymbal context
            if (unk268 != 0) {  // In solo/cymbal section
                void **detector = mTrack ? mTrack->GetSoloDetector() : nullptr;
                if (detector && (*detector)->unk3C(detector, gem_slot, 1) == 0
                    && mBehavior->unk2C() == 0) {
                    mBehavior->unk128();
                    DealWithCodaGem(mBand, (Player *)this, gem_id, 0, 0);
                }
            }

            // Check if should be ignored
            if (IgnoreGemsAt(gem_slot)) {
                IgnoreGem(gem_id);
                return;
            }

            // Check if in ignorable fill
            if (InIgnorableFill(gem_slot)) {
                if (mTrack) {
                    mTrack->Pass(gem_id);
                }
                IgnoreGem(gem_id);
                return;
            }

            // Check ignored flag (0x8)
            if (!mGemStatus->GetIgnored(gem_id)) {
                if (mTrack) {
                    mTrack->Pass(gem_id);
                }

                // Increment pass counter
                mGemStatus->mGems[gem_id] |= 4;  // Mark processed
                UpdateSectionStats();

                // Record in statistics
                PassGem(&mStatCollector, ms, gem, gem_id);

                // Check if cymbal (bit 6)
                if ((gem->unk10 >> 6) & 1) {
                    unk188++;
                }
            }

            // Handle phrase notes (0xE bits)
            int phrase_bits = mGemStatus->mGems[gem_id] & 0xE;
            if (phrase_bits == 0) {  // Not a phrase note
                mGemStatus->mGems[gem_id] |= 4;

                if (ShouldPenalizeGem(gem_id)) {
                    Penalize(ms, gem_id, 0.0f);
                } else {
                    mBehavior->unk58();
                    HandleCommonPhraseNote(0, gem_id);
                }

                mBehavior->unk60(gem_id);
                unk3BC++;
            } else {  // IS a phrase note
                bool is_phrase_start = true;
                if (gem_id != 0) {
                    s32 prev_phrase = TheSongDB->GetPhraseID(mTrackNum, gem_id - 1);
                    if (TheSongDB->GetPhraseID(mTrackNum, gem_id) == prev_phrase) {
                        is_phrase_start = false;
                    }
                }

                bool is_phrase_end = true;
                s32 num_phrases = TheSongDB->GetNumPhraseIDs(mTrackNum);
                if (gem_id != num_phrases - 1) {
                    s32 next_phrase = TheSongDB->GetPhraseID(mTrackNum, gem_id + 1);
                    if (TheSongDB->GetPhraseID(mTrackNum, gem_id) == next_phrase) {
                        is_phrase_end = false;
                    }
                }

                // Check 0x2 flag (missed) and phrase boundaries
                bool has_missed_flag = (mGemStatus->mGems[gem_id] & 0x2) != 0;
                if ((has_missed_flag && is_phrase_start) ||
                    (is_phrase_end && TheGame->unk27 != 0)) {
                    HandleCommonPhraseNote(0, gem_id);
                }

                mGemStatus->mGems[gem_id] |= 4;
            }

            unk18++;

            // Send delta tracking message if enabled
            bool should_send_pass_msg = false;
            if (TheGame->unk24 != 0) {  // Delta tracking enabled
                if ((mGemStatus->mGems[gem_id] & 0x8) != 0) {
                    should_send_pass_msg = true;
                }
            }

            if (!should_send_pass_msg) {
                // Initialize static message on first call
                static bool passMsg_initialized = false;
                if (!passMsg_initialized) {
                    // Build pass message with gem_id parameter
                    // Message("pass", gem_id)
                    passMsg_initialized = true;
                }

                // Send message
                if (mMessageHandler) {
                    mMessageHandler->Send(mMessageContext, 0);  // Pass gem_id
                }
            }

            // Call remote handler
            mBehavior->unk2D4(track, gem_id, cur_track, ms);
        }
    }
}
```

---

## Uncertainties and Questions

1. **Member Variable Names** – Many offsets are unmapped:
   - `unk248`: Likely a track number or song ID for `TheSongDB` queries
   - `unk268`, `unk3E1`, `unk3BC`, `unk188`: Purpose unclear without debug symbols
   - `unk374`: Appears to be a cymbal/solo detector object
   - `unk210`, `unk20C`: Message sending infrastructure
   - `unk4`, `unk58`, `unk60`, `unk128`, `unk2C`, `unk2D4`: Behavior interface methods

2. **Delta Tracking Logic** – The string "delta tracking not enabled\n" and the guard variable suggest this is initialization code for a static message, but the purpose of delta tracking in this context needs clarification.

3. **Gem Status Bits** – Several bit values (0x2, 0xE, 0x40) need verification against actual gameplay behavior:
   - What is 0x40 used for exactly?
   - Are 0x2, 0xD, 0xE part of a phrase note type field?

4. **Phrase Handling** – The logic for phrase start/end detection and the relationship between them and penalty/common phrase handling is complex and may have edge cases.

5. **Cymbal Detection** – The bit shift `(gem->unk10 >> 6) & 1` identifies cymbals, but the meaning of other bits in `unk10` is unknown.

6. **Remote Handler** – The final call to `unk2D4(track, gem_id, cur_track, ms)` appears to notify remote players or systems, but its exact purpose is unclear.

---

## Size Estimate

Current empty stub: 1 line of `MILO_WARN()`
Expected full implementation: ~150-200 lines of C++

The 1564-byte target suggests significant code with:
- Multiple function calls with parameters
- Conditional branches
- Member access patterns
- Static message initialization

The 3.9% match indicates the current stub matches almost nothing, so full implementation should yield much higher match%.

---

## Build Notes

- **Compiler:** MetroWorks CodeWarrior for Wii (mwcceppc v4.3.172)
- **Flags:** `-O4,p -inline noauto -ipa file -sdata 2 -sdata2 2`
- **Float representation:** Uses IEEE 32-bit floats, `f1` register for parameter passing
- **Integer registers:** r3-r6 for first 4 integer arguments, r29-r31 available for saves

The large size (1564 bytes) and complex control flow suggest aggressive inlining of called functions or large inline data structures.

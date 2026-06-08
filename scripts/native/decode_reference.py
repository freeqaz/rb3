#!/usr/bin/env python3
"""
decode_reference.py — INDEPENDENT reference stereo decode of an HMX song.

Builds the "expected stereo" the game SHOULD produce, WITHOUT reusing the game's
decode/mix (which are the suspects in the clipped-noise investigation). Pipeline:

  1. decrypt_mogg.py        -> plaintext multichannel OggS  (standalone AES-CTR)
  2. ffmpeg                 -> per-channel float32 PCM @ source rate
  3. pan/vol downmix        -> reference stereo, applying the SONG's pans/vols
                              with the GAME's actual pan law, but NO hard clamp
                              (headroom preserved) so we can see how badly the
                              real (clamped) output diverges.

Pan law mirrors the native renderer exactly
(milo-native-engine/src/platform/StreamReceiver_Native.cpp:201):
    volL = DbToRatio(vol_db) * max(0, 1 - pan)
    volR = DbToRatio(vol_db) * max(0, 1 + pan)
This is a LINEAR pan law (pan=0 -> unity to BOTH L and R), which is why summing
~15 channels easily exceeds unity and the game's [-1,1] clamp clips. The
reference keeps full headroom (float WAV, no clamp) so divergence is measurable.

Metadata source: orig-assets/extracted/songs/songs.dta  block for <song_id>:
    (song (pans (...)) (vols (...)) (tracks ...))   -- 15 pans, 15 vols (dB)

Usage:
    python3 scripts/native/decode_reference.py 20thcenturyboy
    python3 scripts/native/decode_reference.py 20thcenturyboy --section preview
    python3 scripts/native/decode_reference.py 20thcenturyboy --out-dir /tmp \
            --keep-channels
"""
import argparse, os, re, subprocess, sys, struct, json

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts", "native"))
import decrypt_mogg  # standalone decryptor (validated against native MOGG_DBG)

import numpy as np

SONGS_DIR = os.path.join(REPO, "orig-assets", "extracted", "songs")
SONGS_DTA = os.path.join(SONGS_DIR, "songs.dta")


# ---------------------------------------------------------------------------
# songs.dta metadata
# ---------------------------------------------------------------------------
def _find_block(text, song_id):
    """Return the top-level (<song_id> ...) s-expression block text."""
    needle = "(" + song_id
    i = text.find(needle)
    if i < 0:
        raise KeyError("song id %r not found in songs.dta" % song_id)
    # ensure it's the top-level block opener: '(<id>' followed by whitespace
    while i >= 0:
        nxt = text[i + len(needle)]
        if nxt in " \t\r\n":
            break
        i = text.find(needle, i + 1)
    depth = 0
    j = i
    n = len(text)
    while j < n:
        c = text[j]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
        j += 1
    raise ValueError("unbalanced block for %r" % song_id)


def _floats_after(block, key):
    """Extract the float list following (key (....)) inside block."""
    m = re.search(r"\(%s\s*\(([^)]*)\)" % re.escape(key), block)
    if not m:
        raise KeyError("key %r not found" % key)
    return [float(x) for x in m.group(1).split()]


def _ints_pair(block, key):
    m = re.search(r"\(%s\s+([-\d]+)\s+([-\d]+)\)" % re.escape(key), block)
    if not m:
        return None
    return int(m.group(1)), int(m.group(2))


def _int_val(block, key):
    m = re.search(r"\(%s\s+([-\d]+)\)" % re.escape(key), block)
    return int(m.group(1)) if m else None


def load_song_meta(song_id):
    text = open(SONGS_DTA, "r", encoding="latin-1").read()
    block = _find_block(text, song_id)
    pans = _floats_after(block, "pans")
    vols = _floats_after(block, "vols")
    meta = {
        "song_id": song_id,
        "internal_song_id": _int_val(block, "song_id"),
        "pans": pans,
        "vols": vols,
        "preview_ms": _ints_pair(block, "preview"),
        "song_length_ms": _int_val(block, "song_length"),
    }
    # song path (mogg) — from (song (name "songs/<id>/<id>"))
    m = re.search(r'\(song\s*\(name\s*"([^"]+)"', block)
    meta["song_path"] = m.group(1) if m else "songs/%s/%s" % (song_id, song_id)
    return meta


# ---------------------------------------------------------------------------
# ffmpeg decode -> float32 planar per-channel numpy array (channels, samples)
# ---------------------------------------------------------------------------
def decode_ogg_to_channels(ogg_path):
    """Decode multichannel ogg to float32. Returns (rate, np.array[ch, n])."""
    # probe rate + channels
    probe = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "a:0",
         "-show_entries", "stream=channels,sample_rate", "-of", "json", ogg_path],
        capture_output=True, text=True, check=True)
    info = json.loads(probe.stdout)["streams"][0]
    rate = int(info["sample_rate"])
    ch = int(info["channels"])
    # decode to raw f32le interleaved (ffmpeg won't split >2ch cleanly, so
    # decode interleaved then de-interleave with numpy)
    proc = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", ogg_path,
         "-f", "f32le", "-acodec", "pcm_f32le", "-ac", str(ch), "-"],
        capture_output=True, check=True)
    data = np.frombuffer(proc.stdout, dtype="<f4")
    n = data.size // ch
    data = data[: n * ch].reshape(n, ch).T  # (ch, n)
    return rate, data


# ---------------------------------------------------------------------------
# pan/vol downmix (the game's linear pan law, NO clamp)
# ---------------------------------------------------------------------------
def db_to_ratio(db):
    return 0.0 if db <= -96.0 else 10.0 ** (db / 20.0)


def downmix_reference(channels, pans, vols):
    """channels: (ch, n) float32. Returns stereo (2, n) float64, no clamp."""
    ch, n = channels.shape
    assert len(pans) == ch == len(vols), \
        "pan/vol count (%d/%d) != channel count (%d)" % (len(pans), len(vols), ch)
    L = np.zeros(n, dtype=np.float64)
    R = np.zeros(n, dtype=np.float64)
    per_chan_gain = []
    for i in range(ch):
        vol = db_to_ratio(vols[i])
        pan = pans[i]
        gL = vol * max(0.0, 1.0 - pan)
        gR = vol * max(0.0, 1.0 + pan)
        per_chan_gain.append((gL, gR))
        L += channels[i].astype(np.float64) * gL
        R += channels[i].astype(np.float64) * gR
    return np.vstack([L, R]), per_chan_gain


# ---------------------------------------------------------------------------
# SAME-MIX master bus — a FAITHFUL Python port of the native engine's master DSP
# (milo-native-engine/src/audio/AudioDevice.cpp PumpAudio, lines ~399-423 +
# constants ~33-51). This turns the headroom-preserved no-clamp downmix into the
# game's ACTUAL post-limiter output, so a reference built with it matches the
# captured game audio far more tightly (chroma/fingerprint ceiling rises from a
# moderate rank toward a strong match). Constants are mirrored verbatim:
#
#   sPreGain       = 1.0    (default; DC3_AUDIO_GAIN env override at runtime)
#   kLimThreshold  = 0.90   (begin gain reduction when stereo-linked |peak| > this)
#   kLimReleaseMs  = 80.0   (slow one-pole release, no pumping)
#   INSTANT attack (no kLimAttack constant — the engine comment documents that any
#                   finite attack let the first sample of a fast transient rail, so
#                   the gain drops IMMEDIATELY to the exact value holding the post-
#                   gain sample at threshold: `if desired < env: env = desired`)
#   kSoftKnee      = 0.95   (soft-knee saturator safety net above the knee)
#
# The chain is, per sample, stereo-LINKED (one envelope driven by max(|L|,|R|) so
# the stereo image stays stable):
#   l,r  = L*sPreGain, R*sPreGain
#   level   = max(|l|, |r|)
#   desired = kLimThreshold/level if level > kLimThreshold else 1.0
#   env     = desired                       (instant attack)  if desired < env
#           = aRel*env + (1-aRel)*desired   (one-pole release) otherwise
#   out     = SoftClip(l*env), SoftClip(r*env)
# where aRel = exp(-1 / (rate * kLimReleaseMs/1000)) and env starts at 1.0
# (mLimiterEnv, reset to 1.0 on resume). All math is done in float32 to match the
# engine's float pipeline exactly.
# ---------------------------------------------------------------------------
LIM_PREGAIN = 1.0
LIM_THRESHOLD = 0.90
LIM_RELEASE_MS = 80.0
SOFT_KNEE = 0.95


def soft_clip(a):
    """Vectorised port of AudioDevice.cpp SoftClip() — transparent below kSoftKnee,
    tanh-compresses the region above toward (never reaching) full scale. `a` is a
    float32 array; returns the saturated array (same sign, |out| < 1.0)."""
    a = a.astype(np.float32, copy=False)
    mag = np.abs(a)
    out = a.copy()
    above = mag > SOFT_KNEE
    if np.any(above):
        k = np.float32(SOFT_KNEE)
        one = np.float32(1.0)
        shaped = k + (one - k) * np.tanh((mag[above] - k) / (one - k))
        out[above] = np.sign(a[above]).astype(np.float32) * shaped.astype(np.float32)
    return out


def apply_master_bus(stereo, rate, pre_gain=LIM_PREGAIN):
    """Apply the native master bus (pre-gain + stereo-linked one-pole peak limiter
    with INSTANT attack + soft-knee saturator) to a (2, n) float stereo signal at
    `rate` Hz. Returns the post-limiter (2, n) float32 — the game's actual mix.

    The peak-envelope recurrence (env[f] depends on env[f-1]) is inherently serial,
    so it runs as a per-sample loop; the soft-knee output is then vectorised."""
    L = (stereo[0] * pre_gain).astype(np.float32)
    R = (stereo[1] * pre_gain).astype(np.float32)
    n = L.shape[0]
    level = np.maximum(np.abs(L), np.abs(R))              # stereo-linked peak
    thr = np.float32(LIM_THRESHOLD)
    desired = np.where(level > thr, thr / np.maximum(level, np.float32(1e-30)),
                       np.float32(1.0)).astype(np.float32)
    aRel = np.float32(np.exp(-1.0 / (rate * (LIM_RELEASE_MS / 1000.0))))
    one_minus = np.float32(1.0) - aRel
    env = np.empty(n, dtype=np.float32)
    e = np.float32(1.0)                                   # mLimiterEnv start = 1.0
    d = desired                                           # local alias for speed
    for f in range(n):
        df = d[f]
        if df < e:
            e = df                                        # instant attack
        else:
            e = aRel * e + one_minus * df                 # one-pole release
        env[f] = e
    outL = soft_clip(L * env)
    outR = soft_clip(R * env)
    return np.vstack([outL, outR]).astype(np.float32)


# ---------------------------------------------------------------------------
# WAV I/O + stats (float32 WAV via wave module is awkward; write 16-bit + f32)
# ---------------------------------------------------------------------------
def write_wav_f32(path, stereo, rate):
    """Write a 32-bit float WAV (IEEE float, format tag 3)."""
    data = np.ascontiguousarray(stereo.T.astype("<f4"))  # (n, 2) interleaved
    n_frames = data.shape[0]
    n_ch = data.shape[1]
    byte_rate = rate * n_ch * 4
    block_align = n_ch * 4
    raw = data.tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF")
        f.write(struct.pack("<I", 36 + len(raw)))
        f.write(b"WAVE")
        f.write(b"fmt ")
        f.write(struct.pack("<IHHIIHH", 16, 3, n_ch, rate, byte_rate, block_align, 32))
        f.write(b"data")
        f.write(struct.pack("<I", len(raw)))
        f.write(raw)


def write_wav_s16(path, stereo, rate):
    """Write a clamped 16-bit PCM WAV (for easy listening / comparison)."""
    clipped = np.clip(stereo, -1.0, 1.0)
    data = np.ascontiguousarray((clipped.T * 32767.0).astype("<i2"))
    n_ch = data.shape[1]
    raw = data.tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF")
        f.write(struct.pack("<I", 36 + len(raw)))
        f.write(b"WAVE")
        f.write(b"fmt ")
        f.write(struct.pack("<IHHIIHH", 16, 1, n_ch, rate, rate * n_ch * 2, n_ch * 2, 16))
        f.write(b"data")
        f.write(struct.pack("<I", len(raw)))
        f.write(raw)


def stats(stereo):
    """Return dict of peak, rms, clip_ratio (|x|>=1.0), crest factor (dB)."""
    flat = stereo.reshape(-1)
    peak = float(np.max(np.abs(flat))) if flat.size else 0.0
    rms = float(np.sqrt(np.mean(flat.astype(np.float64) ** 2))) if flat.size else 0.0
    clip = float(np.mean(np.abs(flat) >= 1.0)) if flat.size else 0.0
    crest = 20.0 * np.log10(peak / rms) if rms > 0 and peak > 0 else 0.0
    return {"peak": round(peak, 5), "rms": round(rms, 5),
            "clip_ratio": round(clip, 6), "crest_db": round(crest, 2)}


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="Independent reference stereo decode")
    ap.add_argument("song_id")
    ap.add_argument("--out-dir", default="/tmp")
    ap.add_argument("--section", choices=["full", "gameplay", "preview"],
                    default="gameplay",
                    help="gameplay/full = whole song; preview = the song's "
                         "(preview start end) window")
    ap.add_argument("--keep-channels", action="store_true",
                    help="also write a per-channel multichannel WAV")
    ap.add_argument("--keep-ogg", action="store_true",
                    help="keep the decrypted .ogg")
    ap.add_argument("--same-mix", action="store_true",
                    help="ALSO apply the native engine's master bus (pre-gain + "
                         "stereo-linked one-pole peak limiter + soft-knee saturator, "
                         "a faithful port of AudioDevice.cpp PumpAudio) AFTER the "
                         "pan/vol downmix, and make THAT the primary reference WAV. "
                         "This is the game's ACTUAL post-limiter mix (chroma/fp "
                         "ceiling rises toward a strong match). The no-clamp WAV is "
                         "still written alongside (suffix _noclamp).")
    ap.add_argument("--pre-gain", type=float, default=LIM_PREGAIN,
                    help="pre-limiter trim for --same-mix (mirrors DC3_AUDIO_GAIN; "
                         "default 1.0 = native unity)")
    args = ap.parse_args()

    sid = args.song_id
    meta = load_song_meta(sid)
    mogg = os.path.join(REPO, "orig-assets", "extracted", meta["song_path"] + ".mogg")
    if not os.path.exists(mogg):
        mogg = os.path.join(SONGS_DIR, sid, sid + ".mogg")
    print("[ref] song=%s mogg=%s" % (sid, mogg))
    print("[ref] pans=%s" % meta["pans"])
    print("[ref] vols=%s" % meta["vols"])
    print("[ref] preview_ms=%s song_length_ms=%s"
          % (meta["preview_ms"], meta["song_length_ms"]))

    # 1. standalone decrypt
    pt, info = decrypt_mogg.decrypt_mogg(mogg)
    assert pt[:4] == b"OggS", "decrypt FAILED — not OggS"
    ogg_path = os.path.join(args.out_dir, "rb3_ref_%s.ogg" % sid)
    open(ogg_path, "wb").write(pt)
    print("[ref] decrypt OK (standalone): %s (%d bytes)" % (ogg_path, len(pt)))

    # 2. ffmpeg decode
    rate, channels = decode_ogg_to_channels(ogg_path)
    ch, n = channels.shape
    print("[ref] decoded: rate=%d ch=%d samples=%d (%.2fs)"
          % (rate, ch, n, n / rate))

    # optional section slice (preview window)
    section_tag = args.section
    if args.section == "preview" and meta["preview_ms"]:
        s0 = int(meta["preview_ms"][0] / 1000.0 * rate)
        s1 = int(meta["preview_ms"][1] / 1000.0 * rate)
        channels = channels[:, s0:s1]
        print("[ref] preview slice: %.2fs..%.2fs (%d samples)"
              % (s0 / rate, s1 / rate, channels.shape[1]))

    # 3. pan/vol downmix (no clamp)
    stereo, gains = downmix_reference(channels, meta["pans"], meta["vols"])
    print("[ref] per-channel (gainL,gainR):")
    for i, (gL, gR) in enumerate(gains):
        print("        ch%2d pan=%+.2f vol=%+.1fdB -> L=%.4f R=%.4f"
              % (i, meta["pans"][i], meta["vols"][i], gL, gR))

    # 4. write reference WAVs + stats
    base = "rb3_ref_%s_%s" % (sid, section_tag)

    if args.same_mix:
        # SAME-MIX path: the primary reference is the game's ACTUAL post-limiter mix
        # (port of AudioDevice.cpp's master bus). The no-clamp downmix is kept too,
        # under a *_noclamp suffix, so both can be compared.
        wav_noclamp = os.path.join(args.out_dir, base + "_noclamp.wav")
        write_wav_f32(wav_noclamp, stereo, rate)
        st_noclamp = stats(stereo)
        print("[ref] no-clamp stereo (headroom, float WAV): %s" % wav_noclamp)
        print("[ref]   peak=%.4f rms=%.5f clip_ratio=%.6f crest=%.2fdB"
              % (st_noclamp["peak"], st_noclamp["rms"],
                 st_noclamp["clip_ratio"], st_noclamp["crest_db"]))

        mixed = apply_master_bus(stereo, rate, pre_gain=args.pre_gain).astype(np.float64)
        wav_f32 = os.path.join(args.out_dir, base + ".wav")        # SAME-MIX = primary
        wav_s16 = os.path.join(args.out_dir, base + "_s16.wav")
        write_wav_f32(wav_f32, mixed, rate)
        write_wav_s16(wav_s16, mixed, rate)
        st = stats(mixed)
        print("[ref] SAME-MIX stereo (native master bus, float WAV): %s" % wav_f32)
        print("[ref]   pre_gain=%.2f thr=%.2f releaseMs=%.0f knee=%.2f"
              % (args.pre_gain, LIM_THRESHOLD, LIM_RELEASE_MS, SOFT_KNEE))
        print("[ref]   rate=%d channels=2 peak=%.4f rms=%.5f clip_ratio=%.6f crest=%.2fdB"
              % (rate, st["peak"], st["rms"], st["clip_ratio"], st["crest_db"]))
        print("[ref] clamped 16-bit listening copy: %s" % wav_s16)

        result = {"song_id": sid, "section": section_tag, "rate": rate, "channels": 2,
                  "wav_f32": wav_f32, "wav_s16": wav_s16,
                  "wav_noclamp": wav_noclamp,
                  "decrypt": "standalone", "mix": "same-mix",
                  "master_bus": {"pre_gain": args.pre_gain, "threshold": LIM_THRESHOLD,
                                 "release_ms": LIM_RELEASE_MS, "soft_knee": SOFT_KNEE},
                  "stats": st, "stats_noclamp": st_noclamp,
                  "pans": meta["pans"], "vols": meta["vols"]}
    else:
        wav_f32 = os.path.join(args.out_dir, base + ".wav")          # float, headroom
        wav_s16 = os.path.join(args.out_dir, base + "_s16.wav")      # clamped 16-bit
        write_wav_f32(wav_f32, stereo, rate)
        write_wav_s16(wav_s16, stereo, rate)
        st = stats(stereo)
        print("[ref] reference stereo (no clamp, float WAV): %s" % wav_f32)
        print("[ref]   rate=%d channels=2 peak=%.4f rms=%.5f clip_ratio=%.6f crest=%.2fdB"
              % (rate, st["peak"], st["rms"], st["clip_ratio"], st["crest_db"]))
        print("[ref] clamped 16-bit listening copy: %s" % wav_s16)

        result = {"song_id": sid, "section": section_tag, "rate": rate, "channels": 2,
                  "wav_f32": wav_f32, "wav_s16": wav_s16,
                  "decrypt": "standalone", "mix": "no-clamp", "stats": st,
                  "pans": meta["pans"], "vols": meta["vols"]}

    if args.keep_channels:
        # interleaved float WAV of all channels (post pan-applied? no — raw decode)
        chan_wav = os.path.join(args.out_dir, "rb3_ref_%s_channels.wav" % sid)
        write_wav_f32(chan_wav, channels.astype("<f4"), rate) if ch == 2 else None
        # multichannel >2: write as raw interleaved f32 wav with N channels
        data = np.ascontiguousarray(channels.T.astype("<f4"))
        raw = data.tobytes()
        with open(chan_wav, "wb") as f:
            f.write(b"RIFF"); f.write(struct.pack("<I", 36 + len(raw))); f.write(b"WAVE")
            f.write(b"fmt "); f.write(struct.pack("<IHHIIHH", 16, 3, ch, rate,
                                                  rate * ch * 4, ch * 4, 32))
            f.write(b"data"); f.write(struct.pack("<I", len(raw))); f.write(raw)
        print("[ref] per-channel WAV (%d ch): %s" % (ch, chan_wav))
        result["wav_channels"] = chan_wav

    if not args.keep_ogg:
        try:
            os.remove(ogg_path)
        except OSError:
            pass
    else:
        result["ogg"] = ogg_path

    print("[ref] RESULT_JSON " + json.dumps(result))
    return result


if __name__ == "__main__":
    main()

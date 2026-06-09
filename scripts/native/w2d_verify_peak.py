#!/usr/bin/env python3
"""W2-D adversarial verify: re-derive the un-clamped mix-bus peak under BOTH the
reference's pan law (2x boost on hard-pan) AND the ACTUAL native build's capped
balance law, from the raw 15-channel decode. Also sanity-check the proposed
limiter constants on the reference WAV.

Reads WAV via the `wave` module + numpy only (no soundfile)."""
import sys, wave, struct
import numpy as np

def db_to_ratio(db):
    return 10.0 ** (db / 20.0)

# 20thcenturyboy pans/vols (from phase1-reference.md / songs.dta), 15 channels.
PANS = [-1.0, 1.0, -1.0, 1.0, -1.0, 1.0, 0.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0]
VOLS = [-4.5, -4.5, -4.7, -4.7, -4.5, -4.5, -1.5, -4.5, -4.5, -4.3, -4.3, -4.5, -4.5, -4.5, -4.5]

def read_wav_float(path, max_frames=None):
    """Manual RIFF parse: handles IEEE-float (fmt tag 3) AND int16 WAVs.
    Returns (nchan, samples) float64 array, [-1,1] for int16, native scale for float."""
    with open(path, 'rb') as f:
        riff = f.read(12)
        assert riff[0:4] == b'RIFF' and riff[8:12] == b'WAVE', "not a WAV"
        fmt_tag = nch = rate = bits = None
        data_off = data_len = None
        while True:
            ch = f.read(8)
            if len(ch) < 8:
                break
            cid = ch[0:4]
            csz = struct.unpack('<I', ch[4:8])[0]
            if cid == b'fmt ':
                fmt = f.read(csz)
                fmt_tag, nch, rate, _br, _ba, bits = struct.unpack('<HHIIHH', fmt[:16])
            elif cid == b'data':
                data_off = f.tell()
                data_len = csz
                break
            else:
                f.seek(csz, 1)
        assert data_off is not None, "no data chunk"
        bytes_per_samp = bits // 8
        frame_bytes = bytes_per_samp * nch
        nframes = data_len // frame_bytes
        if max_frames:
            nframes = min(nframes, max_frames)
        f.seek(data_off)
        raw = f.read(nframes * frame_bytes)
    if fmt_tag == 3 and bits == 32:
        a = np.frombuffer(raw, dtype='<f4').astype(np.float64)
    elif fmt_tag == 1 and bits == 16:
        a = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768.0
    elif fmt_tag == 1 and bits == 32:
        a = np.frombuffer(raw, dtype='<i4').astype(np.float64) / 2147483648.0
    else:
        raise RuntimeError("unsupported fmt_tag=%s bits=%s" % (fmt_tag, bits))
    a = a.reshape(-1, nch).T  # (nch, n)
    return a, rate

def downmix_ref_law(ch, pans, vols):
    """Reference's law: gL = vol*max(0,1-pan), gR = vol*max(0,1+pan). 2x at hard-pan."""
    n = ch.shape[1]
    L = np.zeros(n); R = np.zeros(n)
    for i in range(ch.shape[0]):
        v = db_to_ratio(vols[i])
        gL = v * max(0.0, 1.0 - pans[i])
        gR = v * max(0.0, 1.0 + pans[i])
        L += ch[i] * gL
        R += ch[i] * gR
    return L, R

def downmix_native_law(ch, pans, vols):
    """Actual rb3 native ComputePanGains: capped balance.
       left = vol*(pan<=0 ? 1 : 1-pan); right = vol*(pan>=0 ? 1 : 1+pan). max=vol."""
    n = ch.shape[1]
    L = np.zeros(n); R = np.zeros(n)
    for i in range(ch.shape[0]):
        v = db_to_ratio(vols[i])
        p = max(-1.0, min(1.0, pans[i]))
        gL = v * (1.0 if p <= 0 else 1.0 - p)
        gR = v * (1.0 if p >= 0 else 1.0 + p)
        L += ch[i] * gL
        R += ch[i] * gR
    return L, R

def stats(L, R):
    flat = np.concatenate([L, R])
    peak = float(np.max(np.abs(flat)))
    rms = float(np.sqrt(np.mean(flat**2)))
    p99 = float(np.percentile(np.abs(flat), 99))
    over1 = float(np.mean(np.abs(flat) > 1.0))
    crest = 20*np.log10(peak/rms) if rms > 0 else 0
    return dict(peak=peak, rms_db=20*np.log10(rms) if rms>0 else -999,
                p99=p99, over1_pct=over1*100, crest=crest)

def main():
    ch_path = "/tmp/rb3_ref_20thcenturyboy_channels.wav"
    # Read full song (float32; 624MB on disk). Override with arg seconds.
    secs = int(sys.argv[1]) if len(sys.argv) > 1 else 236
    max_frames = 44100 * secs
    print("Reading %s (first %ds)..." % (ch_path, max_frames//44100))
    ch, rate = read_wav_float(ch_path, max_frames=max_frames)
    print("channels=%d rate=%d frames=%d" % (ch.shape[0], rate, ch.shape[1]))
    print("per-channel peaks:", " ".join("%.3f" % np.max(np.abs(ch[i])) for i in range(ch.shape[0])))
    print()

    Lr, Rr = downmix_ref_law(ch, PANS, VOLS)
    Ln, Rn = downmix_native_law(ch, PANS, VOLS)
    sr = stats(Lr, Rr)
    sn = stats(Ln, Rn)
    print("=== REFERENCE PAN LAW (2x hard-pan boost, decode_reference.py) ===")
    print("  peak=%.4f  rms=%.2fdBFS  p99=%.3f  >1.0=%.2f%%  crest=%.1fdB" %
          (sr['peak'], sr['rms_db'], sr['p99'], sr['over1_pct'], sr['crest']))
    print("=== ACTUAL NATIVE PAN LAW (capped balance, rb3_stream_receiver_native.cpp) ===")
    print("  peak=%.4f  rms=%.2fdBFS  p99=%.3f  >1.0=%.2f%%  crest=%.1fdB" %
          (sn['peak'], sn['rms_db'], sn['p99'], sn['over1_pct'], sn['crest']))
    print()

    # With native 1.1x master gain applied (the BROKEN path):
    sn11 = stats(Ln*1.1, Rn*1.1)
    print("=== ACTUAL NATIVE LAW x 1.1 master gain (the broken build) ===")
    print("  peak=%.4f  rms=%.2fdBFS  p99=%.3f  >1.0=%.2f%%  crest=%.1fdB" %
          (sn11['peak'], sn11['rms_db'], sn11['p99'], sn11['over1_pct'], sn11['crest']))
    print()

    # Sanity-check the WINNER limiter on the NATIVE-law mix bus (what the engine sees).
    # one-pole stereo-linked peak limiter, T=0.90, atk=3ms, rel=80ms, makeup=1.0, then ±1 clamp.
    T, atkms, relms = 0.90, 3.0, 80.0
    aAtk = np.exp(-1.0/(rate*atkms/1000.0))
    aRel = np.exp(-1.0/(rate*relms/1000.0))
    env = 1.0
    Lo = np.empty_like(Ln); Ro = np.empty_like(Rn)
    for i in range(Ln.shape[0]):
        peak = max(abs(Ln[i]), abs(Rn[i]))
        target = 1.0 if peak <= T else T/peak
        a = aAtk if target < env else aRel
        env = a*env + (1.0-a)*target
        Lo[i] = Ln[i]*env; Ro[i] = Rn[i]*env
    Lo = np.clip(Lo, -1, 1); Ro = np.clip(Ro, -1, 1)
    sl = stats(Lo, Ro)
    flatL = np.concatenate([Lo, Ro])
    atrail = float(np.mean(np.abs(flatL) >= 0.999))*100
    print("=== WINNER LIMITER on NATIVE-law bus (T=0.90 atk3 rel80) + hard clamp ===")
    print("  peak=%.4f  rms=%.2fdBFS  at-rail(|x|>=0.999)=%.4f%%  crest=%.1fdB" %
          (sl['peak'], sl['rms_db'], atrail, sl['crest']))

    # correlation vs the un-processed native-law bus (faithfulness):
    ref = np.concatenate([Ln, Rn]); proc = flatL
    # both same length & alignment
    corr = float(np.corrcoef(ref, proc)[0,1])
    print("  corr(limited vs unprocessed native bus) = %.4f" % corr)

if __name__ == "__main__":
    main()

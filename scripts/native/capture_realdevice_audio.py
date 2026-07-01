#!/usr/bin/env python3
"""
capture_realdevice_audio.py — REAL-DEVICE native audio verification via snd-aloop / PipeWire.

The sibling capture_gameplay_audio.py captures the engine's INTERNAL mix buffer
(MILO_AUDIO_BACKEND=null + DC3_DUMP_AUDIO writes a WAV from inside MixSources()).
That proves the mixer math but NOT that a real audio *device* plays the bytes.

This script closes that gap: it runs rb3-native against a REAL miniaudio playback
device (MILO_AUDIO_BACKEND=alsa -> miniaudio opens ALSA "default"), and captures
what actually came out of the device by recording a loopback sink's monitor.

TWO loopback transports are supported (auto-detected; --transport to force):

  pipewire  (DEFAULT, works WITHOUT root / without /dev/snd group perms)
      Requires a PipeWire null-sink named by --sink (default: rb3_loop) that is
      ALSA "default" (i.e. PipeWire default.audio.sink == that sink). rb3-native's
      ALSA "default" PCM then routes into it, and we `pw-record` its .monitor.
      Create the sink once if missing:
        pw-cli create-node adapter '{ factory.name=support.null-audio-sink \
            node.name=rb3_loop media.class=Audio/Sink object.linger=true \
            audio.position=[FL,FR] }'
        wpctl set-default <id-of-rb3_loop>     # make it the ALSA "default"

  aloop     (snd-aloop hw: device; needs /dev/snd access — `audio` group or a
            seat ACL. On a headless SSH session with no seat and a user not in
            `audio`, raw hw: is EACCES — use the pipewire transport instead.)
      rb3-native must target the loopback PLAYBACK device. miniaudio has no
      device-id env knob today (AudioDevice::Init never sets
      config.playback.pDeviceID — engine/src/audio/AudioDevice.cpp), so the only
      way to force a specific hw: device without a code change is an ~/.asoundrc
      that makes "default" = the loopback. We capture the loopback CAPTURE side
      with `arecord`/`pw-record`:  arecord -D hw:Loopback,1,0 -f S16_LE -r 48000 -c2

Then audio_verify.py rates the capture against the song's ground-truth mogg.

USAGE
  # PipeWire transport (recommended on this host):
  python3 scripts/native/capture_realdevice_audio.py --secs 12 --out /tmp/rb3_dev.wav
  # then verify:
  python3 scripts/native/audio_verify.py --rank /tmp/rb3_dev.wav --data orig-assets/extracted

This script forwards the same RB3_GAME_INPUT nav-into-gameplay flow as
capture_gameplay_audio.py if --nav is given; by default it just boots and lets
menu/preview audio play (enough to prove the device path end-to-end).
"""
import argparse, os, shutil, signal, subprocess, sys, time, wave, array, math

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")


def have(cmd):
    return shutil.which(cmd) is not None


def pw_default_sink():
    """Return the PipeWire default.audio.sink name, or None."""
    if not have("pw-metadata"):
        return None
    try:
        out = subprocess.run(["pw-metadata"], capture_output=True, text=True,
                             timeout=5).stdout
    except Exception:
        return None
    for line in out.splitlines():
        if "default.audio.sink" in line and "name" in line:
            # value:'{"name":"rb3_loop"}'
            i = line.find('"name":"')
            if i >= 0:
                j = line.find('"', i + 8)
                return line[i + 8:j]
    return None


def wav_stats(path):
    try:
        w = wave.open(path, "rb")
    except Exception as e:
        return f"<unreadable: {e}>"
    n, sr, ch, sw = w.getnframes(), w.getframerate(), w.getnchannels(), w.getsampwidth()
    raw = w.readframes(n)
    w.close()
    if sw == 2:
        a = array.array("h"); a.frombytes(raw)
        rms = math.sqrt(sum((x / 32768.0) ** 2 for x in a) / len(a)) if a else 0
        peak = (max(abs(x) for x in a) / 32768.0) if a else 0
    elif sw == 4:
        a = array.array("f"); a.frombytes(raw)
        rms = math.sqrt(sum(x * x for x in a) / len(a)) if a else 0
        peak = (max(abs(x) for x in a) if a else 0)
    else:
        return f"frames={n} rate={sr} ch={ch} sampwidth={sw} (unsupported width)"
    return (f"frames={n} ch={ch} rate={sr} dur={n / sr:.2f}s "
            f"RMS={rms:.5f} peak={peak:.4f}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/rb3_realdevice.wav")
    ap.add_argument("--secs", type=int, default=12, help="capture seconds")
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--transport", choices=["auto", "pipewire", "aloop"], default="auto")
    ap.add_argument("--sink", default="rb3_loop",
                    help="pipewire loopback sink name (must be the default sink)")
    ap.add_argument("--aloop-capture", default="hw:Loopback,1,0",
                    help="aloop CAPTURE device for arecord/pw-record")
    ap.add_argument("--nav", default=None,
                    help="path to RB3_GAME_INPUT nav script to drive into gameplay")
    ap.add_argument("--port", type=int, default=18760)
    args = ap.parse_args()

    out = os.path.abspath(args.out)

    # Decide transport.
    transport = args.transport
    if transport == "auto":
        ds = pw_default_sink()
        if have("pw-record") and ds == args.sink:
            transport = "pipewire"
        else:
            transport = "aloop"
    print(f"[realdevice] transport={transport}")

    # Build the recorder command.
    if transport == "pipewire":
        ds = pw_default_sink()
        if ds != args.sink:
            print(f"[realdevice] WARNING: PipeWire default sink is {ds!r}, "
                  f"not {args.sink!r}. rb3-native's ALSA 'default' may not route "
                  f"to the captured sink. Set it default with: wpctl set-default <id>")
        rec = ["pw-record", "--target", args.sink,
               "-P", "{ stream.capture.sink=true }", out]
    else:  # aloop
        if have("arecord"):
            rec = ["arecord", "-D", args.aloop_capture, "-f", "S16_LE",
                   "-r", "48000", "-c", "2", "-d", str(args.secs + 2), out]
        elif have("pw-record"):
            rec = ["pw-record", "--target", args.aloop_capture, out]
        else:
            print("[realdevice] ERROR: neither arecord nor pw-record found", file=sys.stderr)
            return 2

    # rb3-native env: REAL alsa device (NOT the null backend, NOT the WAV-dump path).
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(args.port),
        "MILO_HEADLESS": "1", "MILO_AUDIO": "1", "MILO_AUDIO_BACKEND": "alsa",
        "RB3_DATA": args.data,
    })
    if args.nav:
        env["RB3_GAME_INPUT"] = args.nav

    logf = open("/tmp/rb3_realdevice_boot.log", "w")
    print(f"[realdevice] recorder: {' '.join(rec)}")
    recp = subprocess.Popen(rec, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.6)
    print(f"[realdevice] launching rb3-native (MILO_AUDIO_BACKEND=alsa), "
          f"boot log -> /tmp/rb3_realdevice_boot.log")
    rbp = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT)

    try:
        time.sleep(args.secs)
    finally:
        for p in (recp, rbp):
            try:
                p.send_signal(signal.SIGINT)
            except Exception:
                pass
        time.sleep(0.5)
        for p in (recp, rbp):
            try:
                p.kill()
            except Exception:
                pass
    logf.close()

    print("[realdevice] AudioDevice init line(s):")
    try:
        with open("/tmp/rb3_realdevice_boot.log") as f:
            for ln in f:
                if "AudioDevice" in ln or "backend" in ln:
                    print("   ", ln.rstrip())
    except Exception:
        pass

    print(f"[realdevice] capture: {out}")
    print("   ", wav_stats(out))
    print("[realdevice] verify with:")
    print(f"    python3 {os.path.join('scripts','native','audio_verify.py')} "
          f"--rank {out} --data {args.data}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

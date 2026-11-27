"""EXP-93 audio analysis: click -> beep onset delay per tap.

Usage: python latency.py OpenScope Original
Expects OpenScope.wav / Original.wav (mono, 48 kHz) in the current directory. The
EXP-93 recordings are not in the repo; to redo it, record your own and convert, e.g.
  ffmpeg -i <openscope recording>.m4a -ac 1 -ar 48000 OpenScope.wav
  ffmpeg -i <stock recording>.m4a     -ac 1 -ar 48000 Original.wav
Beep bands: OpenScope 2.8-3.7 kHz, stock 1.7-2.1 kHz; clicks: 10-16 kHz transients.
Needs numpy and scipy.
"""
import sys, numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfiltfilt
def env(x, fs, lo, hi, win_ms=2):
    sos = butter(4, [lo, hi], btype='band', fs=fs, output='sos')
    y = sosfiltfilt(sos, x)**2
    w = max(1, int(fs*win_ms/1000))
    return np.sqrt(np.convolve(y, np.ones(w)/w, mode='same'))
cfg = {"OpenScope": (2800, 3700), "Original": (1700, 2100)}
for name in sys.argv[1:]:
    fs, x = wavfile.read(name + ".wav"); x = x.astype(np.float64); x /= np.abs(x).max()
    lo, hi = cfg[name]
    eb = env(x, fs, lo, hi, 5)
    thr = 0.25*np.percentile(eb, 99.5)
    on = eb > thr
    # beep segments >= 60 ms, merge gaps < 40 ms
    idx = np.flatnonzero(np.diff(on.astype(int)) == 1); offs = np.flatnonzero(np.diff(on.astype(int)) == -1)
    segs = []
    for a in idx:
        b = offs[offs > a]
        if len(b) == 0: continue
        b = b[0]
        if segs and a - segs[-1][1] < 0.04*fs: segs[-1][1] = b
        else: segs.append([a, b])
    segs = [s for s in segs if s[1]-s[0] > 0.06*fs]
    # click: broadband transient. Use 10-16 kHz band (away from both tones' strong parts), 1 ms envelope.
    ec = env(x, fs, 10000, 16000, 1)
    base = np.median(ec)
    print(f"== {name}: {len(segs)} beeps (band {lo}-{hi} Hz)")
    lat = []
    for a, b in segs:
        w0 = max(0, a - int(0.8*fs)); w1 = a + int(0.01*fs)
        seg = ec[w0:w1]
        k = int(np.argmax(seg)); peak = seg[k]
        snr = peak/base
        tc = (w0 + k)/fs; tb = a/fs
        d = (tb - tc)*1000
        lat.append((d, snr))
        print(f"  beep at {tb:6.3f}s dur {1000*(b-a)/fs:5.0f} ms | strongest click in prior 800 ms at {tc:6.3f}s (peak/median {snr:5.1f}) -> delay {d:6.1f} ms")
    good = [d for d, s in lat if s > 8]
    if good:
        g = np.array(good); print(f"  clicks with peak/median > 8: n={len(g)} delay median {np.median(g):.0f} ms, min {g.min():.0f}, max {g.max():.0f}")

"""EXP-93 audio analysis: every click and beep edge, to check the click/beep pairing.

Usage: python events.py OpenScope Original
Expects OpenScope.wav / Original.wav (mono, 48 kHz) in the current directory. The
EXP-93 recordings are not in the repo; to redo it, record your own and convert, e.g.
  ffmpeg -i <openscope recording>.m4a -ac 1 -ar 48000 OpenScope.wav
  ffmpeg -i <stock recording>.m4a     -ac 1 -ar 48000 Original.wav
Beep bands: OpenScope 2.8-3.7 kHz, stock 1.7-2.1 kHz; clicks: 10-16 kHz transients.
Needs numpy and scipy.
"""
import sys, numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfiltfilt, find_peaks
def env(x, fs, lo, hi, win_ms):
    sos = butter(4, [lo, hi], btype='band', fs=fs, output='sos')
    y = sosfiltfilt(sos, x)**2; w = max(1, int(fs*win_ms/1000))
    return np.sqrt(np.convolve(y, np.ones(w)/w, mode='same'))
cfg = {"OpenScope": (2800, 3700), "Original": (1700, 2100)}
for name in sys.argv[1:]:
    fs, x = wavfile.read(name + ".wav"); x = x.astype(np.float64); x /= np.abs(x).max()
    ec = env(x, fs, 10000, 16000, 1); base = np.median(ec)
    pk, _ = find_peaks(ec, height=15*base, distance=int(0.15*fs))
    eb = env(x, fs, *cfg[name], 5); on = eb > 0.25*np.percentile(eb, 99.5)
    ev = [(p/fs, "click", ec[p]/base, "on" if on[p] else "") for p in pk]
    d = np.diff(on.astype(int)); 
    ev += [(i/fs, "beep+", 0, "") for i in np.flatnonzero(d == 1)] + [(i/fs, "beep-", 0, "") for i in np.flatnonzero(d == -1)]
    ev.sort()
    print(f"== {name}")
    last = None
    for t, k, s, note in ev:
        if k.startswith("beep") and last == k: continue   # drop chatter
        if k.startswith("beep"): last = k
        print(f"  {t:7.3f}  {k:6s} " + (f"x{s:6.0f} {'(during beep)' if note else ''}" if k == "click" else ""))

"""EXP-50j -- where is the trigger relative to the write pointer, as a function of the level?
2 Hz triangle (33..133) at 0x12, hold read edge+40 (frozen buffer). Per record: pointer k (max jump),
distance back from k to the last rising and last falling crossing of the level L (record scale),
v[k-1], v[k-64]. AUTO with edges vs commits per level (commits > edges = fallback reads present).
"""
import sys, time, re, numpy as np
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600
from exp22_stability import grab_frame
sc = Scope("/dev/ttyACM0"); saved = {}
def st():
    t = sc.cmd("status", timeout=6.0); return int(re.search(r"PC0 edges: (\d+)", t).group(1)), int(re.search(r"SPI3 OK: (\d+)", t).group(1))
jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "3"); jds.freq(2.0, 1); jds.amp(2.0, 1); jds.offset(0.5, 1); jds.output(True, False); jds.close()
sc.cmd("fpga scope timebase 12", timeout=6.0); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga holdread 40"); sc.cmd("fpga postedge 0")
def back_dist(v, k, L):
    r = np.r_[v[k:], v[:k]]                      # un-rotated: r[1023] = newest
    hi = r >= L; rise = np.flatnonzero(~hi[:-1] & hi[1:]) + 1; fall = np.flatnonzero(hi[:-1] & ~hi[1:]) + 1
    return (1024 - rise[-1]) if len(rise) else None, (1024 - fall[-1]) if len(fall) else None
for lvl in (-60, -37, -20, 0, 20, -37):
    r = sc.cmd("fpga scope level %d" % lvl); L = int(re.search(r"code 0x([0-9A-Fa-f]+)", r).group(1), 16); time.sleep(3.0)
    e0, o0 = st(); rows = []
    for i in range(10):
        v = np.asarray(grab_frame(sc)["ch1"], float); saved["L%d_%d" % (L, i)] = v
        dv = np.abs(np.diff(np.r_[v, v[0]])); k = int(np.argmax(dv)) + 1
        br, bf = back_dist(v, k, L); rows.append((k, int(v[(k-1) % 1024]), int(v[(k-64) % 1024]), br, bf)); time.sleep(0.9)
    e1, o1 = st()
    print("level %+4d code %3d  edges +%2d commits +%2d | per record (k, v[k-1], v[k-64], back-to-rise-L, back-to-fall-L):" % (lvl, L, e1-e0, (o1-o0)//2))
    for row in rows: print("      k %4d  v[k-1] %3d  v[k-64] %3d  rise %-5s fall %-5s" % row)
np.savez("/tmp/claude-1000/-home-david-osc/7295c197-e28f-4e77-9e4f-ae5fa413ac94/scratchpad/exp50j_records.npz", **saved)
sc.cmd("fpga scope level 0"); sc.cmd("fpga holdread 0"); sc.cmd("fpga scope timebase 10", timeout=6.0)
jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "0"); jds.freq(201.2, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.close(); sc.close(); print("done")

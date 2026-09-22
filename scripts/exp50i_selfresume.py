"""EXP-50i -- (a) does the engine re-arm by itself?  NORMAL, arming read pushed to edge+5000 ms.
    Model 'read arms':  <= 3 edges in 12 s.   Model 'self-resume at edge+189': ~40 edges at 0x10.
(b) corrected hold-read time edge+40 ms vs the rolling read at edge+250 ms, three codes, body metric.
    Prediction: edge+40 at the floor at 0x10/0x11/0x12 (201.2 Hz and 50 Hz); edge+250 torn at 0x11/0x12.
"""
import sys, time, re, numpy as np
sys.path.insert(0, "/home/david/osc/scripts")
from bench import Scope, JDS6600
from exp22_stability import grab_frame, load_rates
def body_max(v, fs, f):
    w = 2*np.pi*f/fs; n = np.arange(len(v)); A = np.c_[np.sin(w*n), np.cos(w*n), np.ones(len(v))]
    idx = np.arange(128, 1024); c, *_ = np.linalg.lstsq(A[idx], v[idx], rcond=None); r = v - A @ c
    return max(np.sqrt(np.mean(r[i:i+32]**2)) for i in range(128, 1024, 32))
rates = load_rates(); sc = Scope("/dev/ttyACM0")
def st():
    t = sc.cmd("status", timeout=6.0); return int(re.search(r"PC0 edges: (\d+)", t).group(1)), int(re.search(r"SPI3 OK: (\d+)", t).group(1))
jds = JDS6600("/dev/ttyUSB0"); jds.write_raw(21, "0"); jds.freq(201.2, 1); jds.amp(3.0, 1); jds.offset(0.0, 1); jds.output(True, False); jds.close()
sc.cmd("fpga scope vdiv 1 5"); sc.cmd("fpga scope level 0"); sc.cmd("fpga scope trigmode auto"); sc.cmd("fpga scope timebase 10", timeout=6.0); sc.cmd("fpga holdread 40"); sc.cmd("fpga postedge 0"); time.sleep(1)
print("(a) NORMAL at 0x10, sine 201.2 Hz, hold read edge+40")
for label, pe in (("control: arming read derived (fill+230)", 0), ("arming read at edge+5000", 5000), ("control again", 0)):
    sc.cmd("fpga postedge %d" % pe); sc.cmd("fpga scope trigmode auto"); time.sleep(0.5); sc.cmd("fpga scope trigmode normal"); time.sleep(1.5)
    e0, o0 = st(); time.sleep(12.0); e1, o1 = st()
    print("   %-40s edges +%d  commits +%d in 12 s" % (label, e1-e0, (o1-o0)//2))
sc.cmd("fpga postedge 0"); sc.cmd("fpga scope trigmode auto")
print("(b) hold read edge+40 vs edge+250, AUTO, body max (floor: ~6 at 0x10, ~11 at 0x11, ~22 at 0x12 for 201.2 Hz; ~8 for 50 Hz)")
for F in (201.2, 50.0):
    jds = JDS6600("/dev/ttyUSB0"); jds.freq(F, 1); jds.close(); time.sleep(0.5)
    for tb in (0x10, 0x11, 0x12):
        sc.cmd("fpga scope timebase %02x" % tb, timeout=6.0); time.sleep(0.5)
        for hr in (40, 250):
            sc.cmd("fpga holdread %d" % hr); time.sleep(2.5 if tb == 0x12 else 1.5)
            e0, o0 = st(); mx = [body_max(np.asarray(grab_frame(sc)["ch1"], float), rates[tb], F) for _ in range(6)]; e1, o1 = st()
            print("   drive %5.1f Hz tb 0x%02X hold edge+%-3d  body %s  median %.0f   edges +%d commits +%d" % (F, tb, hr, " ".join("%3.0f" % x for x in mx), np.median(mx), e1-e0, (o1-o0)//2))
sc.cmd("fpga holdread 0"); sc.cmd("fpga postedge 0"); sc.cmd("fpga scope timebase 10", timeout=6.0); sc.close(); print("done")

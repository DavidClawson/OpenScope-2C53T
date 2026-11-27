"""EXP-93 addendum bench: poll the shell's `buzzer status` about 10 times a
second and log each line with host time. Usage: buzzer_log.py <seconds> <out>;
stops early when <out>.stop exists. `buzzer status` is a fork shell command
(s00500/OpenScope-2C53T feat/remote-control f9f72d5), not part of #102."""
import os, serial, sys, time
dur = float(sys.argv[1]); outp = sys.argv[2]
s = serial.Serial('/dev/cu.usbmodem0CECB90B58051', 115200, timeout=0.05)
s.reset_input_buffer()
out = open(outp, 'w')
t0 = time.time(); buf = b''; n = 0
while time.time() - t0 < dur and not os.path.exists(outp + '.stop'):
    s.write(b'buzzer status\r\n')
    line = None; deadline = time.time() + 0.3
    while time.time() < deadline and line is None:
        buf += s.read(max(1, s.in_waiting))
        while b'\n' in buf:
            l, buf = buf.split(b'\n', 1)
            l = l.decode(errors='replace').strip()
            if l.startswith('buzzer t='):
                line = l
    if line:
        out.write(f'{time.time() - t0:.3f} {line}\n'); n += 1
        if n % 40 == 0: out.flush()
    time.sleep(0.02)
out.close(); s.close()
print('lines', n)

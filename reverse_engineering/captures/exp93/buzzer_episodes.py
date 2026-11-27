"""EXP-93 addendum bench: split a buzzer_log.py capture into tone episodes
(short=1 runs) and print, per episode, the beep ms (toggle delta), the mute
transitions by tone age and the frames seen. Usage: buzzer_episodes.py <capture>"""
import re, sys
rows = []
for ln in open(sys.argv[1]):
    d = dict(re.findall(r'(\w+)=(\S*)', ln))
    if 't' not in d: continue
    for k in ('t','active','toggles','edges','short','muted','age','vupd','upd','val','cls'):
        d[k] = int(d[k])
    rows.append(d)

def rd(r):
    if r['cls'] == 5: return 'OL'
    if r['val'] < 0: return '-'
    return f"{r['val']/1000:g}{r['unit']}"

i = 0; eps = []
while i < len(rows):
    if rows[i]['short']:
        j = i
        while j + 1 < len(rows) and rows[j + 1]['short']: j += 1
        eps.append((i, j)); i = j + 1
    else:
        i += 1
for n, (a, b) in enumerate(eps, 1):
    ra, rb = rows[a], rows[b]
    t0 = ra['t'] - ra['age']
    # reading before the tone: last distinct reading in the 3 s before
    before = [rd(r) for r in rows[max(0, a - 40):a]]
    prev = before[-1] if before else '?'
    beep_ms = rows[min(b + 1, len(rows) - 1)]['toggles'] - rows[max(a - 1, 0)]['toggles']
    dur = rb['t'] - t0
    # mute transitions by tone age
    trans = []; m = 0
    for r in rows[a:b + 1]:
        if r['muted'] != m:
            trans.append(f"{'mute' if r['muted'] else 'unmute'}@{r['age']}")
            m = r['muted']
    # readings in the episode, run-length
    seq = []
    for r in rows[a:b + 1]:
        v = rd(r)
        if not seq or seq[-1][0] != v: seq.append([v, r['age']])
    seqs = ' '.join(f"{v}@{age}" for v, age in seq)
    print(f"#{n} t0={t0} tone~{dur}ms beep={beep_ms}ms before={prev} {' '.join(trans) or 'never muted'}")
    print(f"    frames: {seqs}")
# readings while no tone, compressed
print('no-tone readings (run-length, device t):')
seq = []
for r in rows:
    if r['short']: 
        if seq and seq[-1][0] != 'TONE': seq.append(['TONE', r['t']])
        continue
    v = rd(r)
    if not seq or seq[-1][0] != v: seq.append([v, r['t']])
print('   ', ' '.join(f"{v}@{t}" for v, t in seq))

"""Iterate function_extent over unresolved targets until closed: unknown tail targets that lie
outside every .pdata function become new entries. usage: closure_extent.py <decoded dir> addr..."""
import subprocess, sys, re, json, bisect
from pathlib import Path
d = sys.argv[1]
pd = sorted((int(x['address'],16), x['bytes']) for x in json.load(open(Path(d)/'pdata-functions.json')))
st = [a for a, _ in pd]
def inside(t):
    i = bisect.bisect_right(st, t) - 1
    return i >= 0 and st[i] < t < st[i] + pd[i][1]
todo = [a.upper() for a in sys.argv[2:]]
for _ in range(20):
    out = subprocess.run([sys.executable, str(Path(__file__).with_name('function_extent.py')), d] + todo,
                         capture_output=True, text=True).stdout
    new = []
    for m in re.finditer(r'unknown ([0-9A-F]{8})', out):
        t = m.group(1)
        if t not in todo:
            if inside(int(t, 16)): print(f'# {t} is inside a .pdata function'); continue
            new.append(t)
    if not new: break
    todo += new
print(out, end='')

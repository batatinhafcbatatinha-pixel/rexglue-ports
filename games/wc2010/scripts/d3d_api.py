"""List functions inside [lo,hi) called (bl) from outside it, with caller counts."""
import sys
from s4img import *
lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
ext, internal, callers = {}, {}, {}
for a in range(TEXT_LO, TEXT_HI, 4):
    r = bl_target(a)
    if not r or not r[1]:
        continue
    t = r[0]
    if not (lo <= t < hi):
        continue
    if lo <= a < hi:
        internal[t] = internal.get(t, 0) + 1
    else:
        ext[t] = ext.get(t, 0) + 1
        callers.setdefault(t, set()).add(func_of(a))
for t in sorted(ext):
    print("sub_%08X size=%5d ext=%4d callers=%4d int=%d" % (t, func_end(t) - t, ext[t], len(callers[t]), internal.get(t, 0)))
print(sum(1 for f in STARTS if lo <= f < hi), "functions in range;", len(ext), "called externally", file=sys.stderr)

import sys
from s4img import *
for arg in sys.argv[1:]:
    a = int(arg, 16)
    f = func_of(a) if a not in STARTS else a
    print("== sub_%08X (size %d)" % (f, func_end(f) - f))
    print(dis(f))

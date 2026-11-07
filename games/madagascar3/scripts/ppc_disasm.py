import sys,capstone
from pathlib import Path
b=(Path(__file__).resolve().parents[1]/"analysis/decoded/image-mapped.bin").read_bytes()
c=capstone.Cs(capstone.CS_ARCH_PPC,capstone.CS_MODE_32|capstone.CS_MODE_BIG_ENDIAN)
for spec in sys.argv[1:]:
 a,n=spec.split(":") if ":" in spec else (spec,"64")
 a,n=int(a,16),int(n,0)
 print("\nADDRESS",hex(a))
 for i in c.disasm(b[a-0x82000000:a-0x82000000+n*4],a): print(f"{i.address:08X} {int.from_bytes(i.bytes,'big'):08X} {i.mnemonic} {i.op_str}")

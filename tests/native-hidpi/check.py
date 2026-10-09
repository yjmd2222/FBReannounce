# Execute exact old/new x86-64 bytes with relocated read-only constants.
import ctypes,mmap,re,struct,random
from pathlib import Path
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from capstone.x86 import X86_OP_MEM,X86_REG_RIP
root=Path(__file__).resolve().parents[2]
s=(root/'FBReannounce/NativeHiDPICheck.hpp').read_text()
def pattern(name):return bytes(int(v,16) for v in re.findall(r'0x([0-9a-f]{2})',re.search(name+r'\[\]\s*\{(.*?)\};',s,re.S).group(1)))
old,new=pattern('findBytes'),pattern('replBytes');assert len(old)==len(new)==125
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
prefix=bytes.fromhex('55 4889e5 4883ec40 897dc8 8975cc 668955dc 66894dde 31c0')
spaces=[]
def function(block):
 b=bytearray(block);constant_start=len(prefix)+125+4;relocs=0
 for ins in md.disasm(block,0):
  for op in ins.operands:
   if op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP:
    struct.pack_into('<i',b,ins.address+ins.disp_offset,constant_start+8*relocs-(len(prefix)+ins.address+ins.size));relocs+=1
 assert relocs==2
 raw=prefix+b+bytes.fromhex('31c0 c9 c3')+struct.pack('<dd',25.4,160.0)
 space=mmap.mmap(-1,4096,prot=mmap.PROT_READ|mmap.PROT_WRITE);space.write(raw);addr=ctypes.addressof(ctypes.c_char.from_buffer(space));lib=ctypes.CDLL(None);assert lib.mprotect(ctypes.c_void_p(addr),4096,mmap.PROT_READ|mmap.PROT_EXEC)==0
 spaces.append(space)
 return ctypes.CFUNCTYPE(ctypes.c_uint8,*([ctypes.c_uint32]*4))(addr)
a,b=function(old),function(new)
assert a(2736,1824,260,170)==1 and a(1824,2736,170,260)==0
assert b(2736,1824,260,170)==b(1824,2736,170,260)==1
rng=random.Random(1012)
cases=[(w,h,x,y) for w,h in [(0,0),(2047,1536),(2048,1535),(2048,1536),(1536,2048),(65535,1),(0xffffffff,2048)] for x,y in [(0,170),(260,0),(260,170),(65535,65535)]]
cases += [(rng.randrange(8192),rng.randrange(8192),rng.randrange(601),rng.randrange(601)) for _ in range(20000)]
for w,h,x,y in cases:assert b(w,h,x,y)==a(max(w,h),min(w,h),x,y),(w,h,x,y)
print('Exact125-byte predicate: landscape preserved, portrait eligibility fixed, density/zero-size/boundary cases passed;20000 randomized comparisons passed.')

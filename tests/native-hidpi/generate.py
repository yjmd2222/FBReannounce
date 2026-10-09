from pathlib import Path
import struct
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from capstone.x86 import X86_OP_MEM,X86_REG_RIP
import argparse, subprocess, tempfile
parser=argparse.ArgumentParser(description='Generate the Ventura 13.7.8 orientation predicate from an extracted CoreDisplay image.')
parser.add_argument('coredisplay', type=Path)
args=parser.parse_args()
r=Path(__file__).resolve().parents[2]
obj=Path(tempfile.gettempdir())/'fbreannounce-native-hidpi-check.o'
subprocess.run(['clang','-c',str(Path(__file__).with_name('rotated-check.s')),'-o',str(obj)],check=True)
def segments(path):
 d=Path(path).read_bytes();pos=32;segs=[];sections={}
 for _ in range(struct.unpack_from('<I',d,16)[0]):
  cmd,size=struct.unpack_from('<II',d,pos)
  if cmd==0x19:
   segs.append(struct.unpack_from('<QQQQ',d,pos+24))
   for i in range(struct.unpack_from('<I',d,pos+64)[0]):
    q=pos+72+i*80;name=d[q:q+16].split(b'\0')[0];sections[name]=struct.unpack_from('<QQI',d,q+32)
  pos+=size
 return d,segs,sections
d,segs,_=segments(args.coredisplay)
def read(a,n):
 vm,_,o,sz=next(x for x in segs if x[0]<=a<x[0]+x[3]);return d[o+a-vm:o+a-vm+n]
base=0x7ff801e8c169;old=read(base,125)
d2,_,sections=segments(obj);a,n,o=sections[b'__text'];new=bytearray(d2[o:o+125]);assert len(new)==125
# Resolve local short-branch relocations before using raw object bytes.
for offset, opcode, target in [(18,0x72,125),(26,0x72,125),(34,0x74,125),(42,0x74,125),(116,0xeb,127)]:
 assert new[offset]==opcode
 new[offset+1]=target-offset-2
assert new[8:10]==bytes([0x73,2])
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
constants=[]
for ins in md.disasm(old,base):
 for op in ins.operands:
  if op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP:
   target=ins.address+ins.size+op.mem.disp;constants.append((target,read(target,8)))
assert len(constants)==2
# New loads refer to the same existing cache constants; fix only displacement fields.
relocs=[]
for ins in md.disasm(bytes(new),base):
 for op in ins.operands:
  if op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP:
   j=len(relocs);target,value=constants[j];offset=ins.address-base+ins.disp_offset
   struct.pack_into('<i',new,offset,target-(ins.address+ins.size));relocs.append((offset,ins.address-base+ins.size,value.hex()))
assert len(relocs)==2
out=r/'FBReannounce/NativeHiDPICheck.hpp'
fmt=lambda b: ', '.join('0x%02x'%v for v in b)
out.write_text('// SPDX-License-Identifier: GPL-2.0-only\n// Copyright 2026 yjmd2222. Ventura 13.7.8 CoreDisplay Retina eligibility.\n// Sort pixel dimensions for threshold checks; preserve physical-density calculation.\n#pragma once\n#include <stdint.h>\nnamespace NativeHiDPICheck {\nstatic const uint8_t findBytes[] {\n'+fmt(old)+'\n};\nstatic const uint8_t replBytes[] {\n'+fmt(new)+'\n};\nstatic_assert(sizeof(findBytes)==sizeof(replBytes),"patch sizes must match");\n}\n')
import json
(r/'tests/native-hidpi/relocations.json').write_text(json.dumps({'constants':relocs,'span':125},indent=2)+'\n')
print('Generated125-byte predicate correction; constants:',[(hex(a),struct.unpack('<d',v)[0]) for a,v in constants])

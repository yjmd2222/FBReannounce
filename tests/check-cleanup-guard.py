#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
import pathlib, re, struct, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[1]
header = (root / 'FBReannounce/CleanupGuard.hpp').read_text()
def data(name):
    match = re.search(r'\b' + name + r'\[\]\s*\{(.*?)\};', header, re.S)
    return bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]{2})', match.group(1)))
original, patched = data('findBytes'), data('replBytes')
assert len(original) == len(patched) == 42
# Exercise the exact bytes, not a C++ reimplementation. The unchanged release
# continuation is replaced by a flag store at the same +42/+59 boundaries.
def wrapper(name, pattern):
    return f'''
.globl _{name}
_{name}:
 pushq %rbp
 movq %rsp, %rbp
 pushq %r14
 subq $6552, %rsp
 movq %rdi, %r14
 movq $0, -6520(%rbp)
 .byte {','.join(hex(v) for v in pattern)}
 movl $1, -6516(%rbp)
 .fill 7,1,0x90
 movq -6520(%rbp), %rax
 addq $6552, %rsp
 popq %r14
 popq %rbp
 retq
'''
clang = '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'
with tempfile.TemporaryDirectory(prefix='ws-cleanup-test-') as tmp:
    asm = pathlib.Path(tmp) / 'guards.s'
    asm.write_text('.text\n' + wrapper('original_cleanup', original) + wrapper('guarded_cleanup', patched))
    exe = pathlib.Path(tmp) / 'test'
    subprocess.run([clang, '-std=c++14', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(root / 'tests/cleanup-guard-tests.cpp'), str(asm), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
cache = pathlib.Path('/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld/dyld_shared_cache_x86_64h')
if cache.exists():
    # Exact tested Ventura mapping, fail if this cache has a different layout.
    with cache.open('rb') as f:
        h = f.read(32); offset, count = struct.unpack_from('<II', h, 16)
        f.seek(offset); mappings = [struct.unpack('<QQQII', f.read(32)) for _ in range(count)]
        address, size = 0x7ff805075000, 0x3f3ffa
        mapping = next(m for m in mappings if m[0] <= address and address + size <= m[0] + m[1])
        f.seek(mapping[2] + address - mapping[0]); text = f.read(size)
    # The active Lilu cache patch can be visible through this file mapping.
    matches = [(text.count(p), text.find(p)) for p in (original, patched)]
    assert sum(n for n, _ in matches) == 1 and any(n == 1 and offset == 0x10bbb6 for n, offset in matches)
    assert (0x10bbb6 & 0xfff) + len(original) <= 4096
    print('Unique original/applied pattern in tested SkyLight; patch fits one validation page')

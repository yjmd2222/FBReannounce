.text
.globl _rotated_check_bytes
_rotated_check_bytes:
 movl -56(%rbp), %ecx
 movl -52(%rbp), %edx
 cmpl %edx, %ecx
 .byte 0x73
 .byte 1f-.-1
 xchgl %ecx, %edx
1:
 cmpl $2048, %ecx
 .byte 0x72
 .byte .Lfalse-.-1
 cmpl $1536, %edx
 .byte 0x72
 .byte .Lfalse-.-1
 movzwl -36(%rbp), %esi
 testl %esi,%esi
 .byte 0x74
 .byte .Lfalse-.-1
 movzwl -34(%rbp), %edi
 testl %edi,%edi
 .byte 0x74
 .byte .Lfalse-.-1
 imull %ecx,%ecx
 imull %edx,%edx
 addl %ecx,%edx
 cvtsi2sdq %rdx,%xmm0
 sqrtsd %xmm0,%xmm0
 cvtsi2sdl %edi,%xmm1
 cvtsi2sdl %esi,%xmm2
 movsd .L25(%rip),%xmm3
 divsd %xmm3,%xmm2
 divsd %xmm3,%xmm1
 mulsd %xmm1,%xmm1
 mulsd %xmm2,%xmm2
 addsd %xmm1,%xmm2
 sqrtsd %xmm2,%xmm1
 divsd %xmm1,%xmm0
 ucomisd .Lppi(%rip),%xmm0
 seta %al
 .byte 0xeb
 .byte .Ldone-.-1
 .fill 125-(.-_rotated_check_bytes),1,0x90
.Lfalse:
 xorl %eax,%eax
.Ldone:
 leave
 ret
.L25:
 .double 25.4
.Lppi:
 .double 160

.section .text.init
.global _start
_start:
   csrr     a0, mhartid
.option push // TODO: what does this do?
.option norelax // TODO: what does this do?
   la       sp, _stack_end
   la       gp, __global_pointer$
.option pop // TODO: what does this do?
   li       t0, 4096
   mul      t0, a0, t0
   sub      sp, sp, t0

   tail main

.global sbi_hart_hang
sbi_hart_hang:
   wfi
   j        sbi_hart_hang


